#pragma once

#include <Base/Memory/Bytebuffer.h>
#include <MetaGen/Postgres/DatabaseBundle.h>

#include <pqxx/pqxx>
#include <pqxx/cursor>

#include <cstring>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace Database::Generated
{
    inline void AppendParameter(pqxx::params& parameters, const Bytebuffer& value)
    {
        auto& readable = const_cast<Bytebuffer&>(value);
        parameters.append(pqxx::binary_cast(readable.GetDataPointer(), value.writtenData));
    }
    inline void AppendParameter(pqxx::params& parameters, Bytebuffer& value) { AppendParameter(parameters, static_cast<const Bytebuffer&>(value)); }
    inline void AppendParameter(pqxx::params& parameters, Bytebuffer&& value) { AppendParameter(parameters, static_cast<const Bytebuffer&>(value)); }

    inline void AppendParameter(pqxx::params& parameters, const std::optional<Bytebuffer>& value)
    {
        if (value)
            AppendParameter(parameters, *value);
        else
            parameters.append();
    }
    inline void AppendParameter(pqxx::params& parameters, std::optional<Bytebuffer>& value) { AppendParameter(parameters, static_cast<const std::optional<Bytebuffer>&>(value)); }
    inline void AppendParameter(pqxx::params& parameters, std::optional<Bytebuffer>&& value) { AppendParameter(parameters, static_cast<const std::optional<Bytebuffer>&>(value)); }

    template <typename Value>
    void AppendParameter(pqxx::params& parameters, Value&& value)
    {
        parameters.append(std::forward<Value>(value));
    }

    template <typename... Args>
    pqxx::params MakeParameters(Args&&... args)
    {
        pqxx::params parameters;
        parameters.reserve(sizeof...(Args));
        (AppendParameter(parameters, std::forward<Args>(args)), ...);
        return parameters;
    }

    template <typename Schema>
    void PrepareStatements(pqxx::connection& connection)
    {
        std::apply([&](auto... statement)
        {
            (connection.prepare(std::string(decltype(statement)::PREPARED_NAME), std::string(decltype(statement)::SQL)), ...);
        }, typename Schema::PreparedStatements{});
    }

    inline Bytebuffer DecodeBytea(const pqxx::field& field)
    {
        const pqxx::bytes bytes = field.as<pqxx::bytes>();
        Bytebuffer result(nullptr, bytes.size());
        result.SetOwnership(true);
        if (!bytes.empty())
        {
            std::memcpy(result.GetDataPointer(), bytes.data(), bytes.size());
            result.SkipWrite(bytes.size());
        }
        return result;
    }

    template <typename Descriptor>
    auto Decode(const pqxx::row& row)
    {
        if constexpr (requires { Descriptor::Decode(row, DecodeBytea); })
            return Descriptor::Decode(row, DecodeBytea);
        else
            return Descriptor::Decode(row);
    }

    template <typename Table, typename Callback>
    void ForEach(pqxx::transaction_base& transaction, Callback&& callback)
    {
        constexpr pqxx::result::difference_type BatchSize = 512;
        pqxx::stateless_cursor<pqxx::cursor_base::read_only, pqxx::cursor_base::owned> cursor(
            transaction, Table::SELECT_ALL_SQL, "metagen_table_stream", false);
        const auto rowCount = static_cast<pqxx::result::difference_type>(cursor.size());
        for (pqxx::result::difference_type offset = 0; offset < rowCount; offset += BatchSize)
        {
            const auto batch = cursor.retrieve(offset, std::min(offset + BatchSize, rowCount));
            for (const auto& row : batch)
                callback(Decode<Table>(row));
        }
    }

    template <typename Table>
    std::vector<typename Table::Record> LoadAll(pqxx::transaction_base& transaction)
    {
        std::vector<typename Table::Record> records;
        ForEach<Table>(transaction, [&records](typename Table::Record record) { records.emplace_back(std::move(record)); });
        return records;
    }

    template <typename Descriptor, typename... Args>
    auto Execute(pqxx::transaction_base& transaction, Args&&... args)
    {
        static_assert(sizeof...(Args) == std::tuple_size_v<typename Descriptor::Parameters>,
            "Generated operation argument count does not match Descriptor::Parameters");
        const auto statement = pqxx::prepped{ pqxx::zview{ Descriptor::PREPARED_NAME } };
        const auto result = transaction.exec(statement, MakeParameters(std::forward<Args>(args)...));

        if constexpr (!requires { typename Descriptor::Record; })
        {
            return result;
        }
        else if constexpr (Descriptor::CARDINALITY == MetaGen::Postgres::QueryCardinality::ExactlyOne)
        {
            if (result.size() != 1)
                throw std::runtime_error("Generated operation expected exactly one row");
            return Decode<Descriptor>(result.front());
        }
        else if constexpr (Descriptor::CARDINALITY == MetaGen::Postgres::QueryCardinality::ZeroOrOne)
        {
            if (result.size() > 1)
                throw std::runtime_error("Generated operation expected at most one row");
            if (result.empty())
                return std::optional<typename Descriptor::Record>{};
            return std::optional<typename Descriptor::Record>{ Decode<Descriptor>(result.front()) };
        }
        else
        {
            static_assert(Descriptor::CARDINALITY == MetaGen::Postgres::QueryCardinality::ZeroOrMore,
                "Generated operation has an unsupported query cardinality");
            std::vector<typename Descriptor::Record> records;
            records.reserve(result.size());
            for (const auto& row : result)
                records.emplace_back(Decode<Descriptor>(row));
            return records;
        }
    }
}
