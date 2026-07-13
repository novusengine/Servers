#pragma once

#include <Base/Types.h>

#include <optional>
#include <string_view>
#include <utility>

namespace Database
{
    enum class OperationFailure : u8
    {
        None,
        Unavailable,
        Conflict,
        Indeterminate,
        Rejected,
        Failed
    };

    constexpr std::string_view OperationFailureName(OperationFailure failure)
    {
        switch (failure)
        {
            case OperationFailure::None: return "None";
            case OperationFailure::Unavailable: return "Unavailable";
            case OperationFailure::Conflict: return "Conflict";
            case OperationFailure::Indeterminate: return "Indeterminate";
            case OperationFailure::Rejected: return "Rejected";
            case OperationFailure::Failed: return "Failed";
        }
        return "Unknown";
    }

    template <typename T>
    class OperationResult
    {
    public:
        OperationResult(T value) : _value(std::move(value)) {}
        OperationResult(OperationFailure failure) : _failure(failure) {}

        explicit operator bool() const { return _value.has_value(); }
        T& Value() & { return _value.value(); }
        const T& Value() const& { return _value.value(); }
        T&& Value() && { return std::move(_value).value(); }
        OperationFailure Failure() const { return _failure; }

    private:
        std::optional<T> _value;
        OperationFailure _failure = OperationFailure::None;
    };
}
