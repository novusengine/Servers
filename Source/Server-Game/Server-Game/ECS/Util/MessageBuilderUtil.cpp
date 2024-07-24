#include "MessageBuilderUtil.h"
#include "Server-Game/ECS/Components/CombatLog.h"
#include "Server-Game/ECS/Components/UnitStatsComponent.h"

#include <entt/entt.hpp>

namespace ECS::Util::MessageBuilder
{
	namespace Authentication
	{
		bool BuildConnectedMessage(Network::SocketID socketID, std::shared_ptr<Bytebuffer>& buffer, u8 result, entt::entity entity, const std::string* resultString)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 64>();
			}

			switch (result)
			{
				case 0:
				{
					Network::PacketHeader header =
					{
						.opcode = Network::Opcode::SMSG_CONNECTED,
						.size = sizeof(u8) + sizeof(entt::entity) + static_cast<u8>(resultString->size()) + 1
					};

					if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
						return false;

					buffer->Put(header);
					buffer->PutU8(result);
					buffer->Put(entity);
					buffer->PutString(*resultString);

					break;
				}

				default:
				{
					static const std::string CharacterDoesNotExist = "Character does not exist";
					static const std::string AlreadyConnectedToACharacter = "Already connected to a character";
					static const std::string CharacterInUse = "Character is in use";
					static const std::string EmptyStr = "";

					const std::string* errorText = &EmptyStr;
					switch (result)
					{
						case 0x1:
						{
							errorText = &CharacterDoesNotExist;
							break;
						}

						case 0x2:
						{
							errorText = &AlreadyConnectedToACharacter;
							break;
						}

						case 0x4:
						{
							errorText = &CharacterInUse;
							break;
						}
					}

					Network::PacketHeader header =
					{
						.opcode = Network::Opcode::SMSG_CONNECTED,
						.size = sizeof(u8) + static_cast<u8>(errorText->size()) + 1
					};

					if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
						return false;

					buffer->Put(header);
					buffer->PutU8(result);
					buffer->PutString(*errorText);

					break;
				}
			}

			return true;
		}
	}

	namespace Entity
	{
		bool BuildEntityMoveMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const vec3& position, const quat& rotation, const Components::MovementFlags& movementFlags, f32 verticalVelocity)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 40>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::MSG_ENTITY_MOVE,
				.size = sizeof(entt::entity) + sizeof(vec3) + sizeof(quat) + sizeof(Components::MovementFlags) + sizeof(f32)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(entity);
			buffer->Put(position);
			buffer->Put(rotation);
			buffer->Put(movementFlags);
			buffer->Put(verticalVelocity);

			return true;
		}
		bool BuildEntityCreateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::Transform& transform, u32 displayID)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 48>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_ENTITY_CREATE,
				.size = sizeof(entt::entity) + sizeof(u32) + sizeof(Components::Transform)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(entity);
			buffer->PutU32(displayID);
			buffer->Put(transform);

			return true;
		}
		bool BuildEntityDestroyMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 4>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_ENTITY_DESTROY,
				.size = sizeof(entt::entity)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(entity);

			return true;
		}
		bool BuildUnitStatsMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, const Components::UnitStatsComponent& unitStatsComponent)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<(144)>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_ENTITY_RESOURCES_UPDATE,
				.size = sizeof(entt::entity) + sizeof(u32) + sizeof(f32) + sizeof(f32) + sizeof(f32)
			};

			if (buffer->GetSpace() < (u32)Components::PowerType::Count + 1 * (sizeof(Network::PacketHeader) + header.size))
				return false;

			buffer->Put(header);
			buffer->Put(entity);
			buffer->Put(Components::PowerType::Health);
			buffer->PutF32(unitStatsComponent.baseHealth);
			buffer->PutF32(unitStatsComponent.currentHealth);
			buffer->PutF32(unitStatsComponent.maxHealth);


			for (i32 i = 0; i < (u32)Components::PowerType::Count; i++)
			{
				Components::PowerType powerType = (Components::PowerType)i;

				f32 powerValue = unitStatsComponent.currentPower[i];
				f32 basePower = unitStatsComponent.basePower[i];
				f32 maxPower = unitStatsComponent.maxPower[i];

				buffer->Put(header);
				buffer->Put(entity);
				buffer->Put(powerType);
				buffer->PutF32(basePower);
				buffer->PutF32(powerValue);
				buffer->PutF32(maxPower);
			}

			return true;
		}
		bool BuildEntityTargetUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, entt::entity target)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 8>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::MSG_ENTITY_TARGET_UPDATE,
				.size = sizeof(entt::entity) + sizeof(entt::entity)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(entity);
			buffer->Put(target);

			return true;
		}
		bool BuildEntitySpellCastMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, f32 castTime, f32 castDuration)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 12>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_ENTITY_CAST_SPELL,
				.size = sizeof(entt::entity) + sizeof(f32) + sizeof(f32)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(entity);
			buffer->PutF32(castTime);
			buffer->PutF32(castDuration);

			return true;
		}
		bool BuildEntityDisplayInfoUpdateMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u32 displayID)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 8>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_ENTITY_DISPLAYINFO_UPDATE,
				.size = sizeof(entt::entity) + sizeof(u32)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(entity);
			buffer->PutU32(displayID);

			return true;
		}
	}

	namespace Spell
	{
		bool BuildSpellCastResultMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity entity, u8 result, f32 castTime, f32 castDuration)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 65>();
			}

			if (result == 0)
			{
				Network::PacketHeader header =
				{
					.opcode = Network::Opcode::SMSG_SEND_SPELLCAST_RESULT,
					.size = sizeof(u8) + sizeof(f32) + sizeof(f32)
				};

				if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
					return false;

				buffer->Put(header);
				buffer->PutU8(result);
				buffer->PutF32(castTime);
				buffer->PutF32(castDuration);
			}
			else
			{
				static const std::string AlreadyCasting = "A spell is already being cast";
				static const std::string NotTargetAvailable = "You have no target to cast on";
				static const std::string EmptyStr = "";

				const std::string* resultString = &EmptyStr;
				switch (result)
				{
					case 0x1:
					{
						resultString = &AlreadyCasting;
						break;
					}

					case 0x2:
					{
						resultString = &NotTargetAvailable;
						break;
					}

					default: break;
				}

				Network::PacketHeader header =
				{
					.opcode = Network::Opcode::SMSG_SEND_SPELLCAST_RESULT,
					.size = sizeof(u8) + static_cast<u16>(resultString->size()) + 1
				};

				if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
					return false;

				buffer->Put(header);
				buffer->PutU8(result);
				buffer->PutString(*resultString);
			}

			return true;
		}
	}

	namespace CombatLog
	{
		bool BuildDamageDealtMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target, f32 damage)
		{
			if (buffer == nullptr)
            {
                buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 14>();
            }

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_COMBAT_EVENT,
				.size = sizeof(u16) + sizeof(entt::entity) + sizeof(entt::entity) + sizeof(f32)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(Components::CombatLogEvents::DamageDealt);
			buffer->Put(source);
			buffer->Put(target);
			buffer->PutF32(damage);

			return true;
		}
		bool BuildHealingDoneMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target, f32 healing)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 14>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_COMBAT_EVENT,
				.size = sizeof(u16) + sizeof(entt::entity) + sizeof(entt::entity) + sizeof(f32)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(Components::CombatLogEvents::HealingDone);
			buffer->Put(source);
			buffer->Put(target);
			buffer->PutF32(healing);

			return true;
		}
		bool BuildRessurectedMessage(std::shared_ptr<Bytebuffer>& buffer, entt::entity source, entt::entity target)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 10>();
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_COMBAT_EVENT,
				.size = sizeof(u16) + sizeof(entt::entity) + sizeof(entt::entity)
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->Put(Components::CombatLogEvents::Ressurected);
			buffer->Put(source);
			buffer->Put(target);

			return true;
		}
	}

	namespace Cheat
	{
		bool BuildCharacterCreateResultMessage(std::shared_ptr<Bytebuffer>& buffer, const std::string& name, u8 result)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 53>();
			}

			static const std::string InvalidNameSupplied = "Character creation failed due to invalid name";
			static const std::string CharacterWasCreated = "Character was created";
			static const std::string CharacterAlreadyExists = "A character with the given name already exists";
			static const std::string DatabaseTransactionFailed = "A database error occured during character creation";
			static const std::string EmptyStr = "";

			const std::string* resultString = &EmptyStr;
			switch (result)
			{
				case 0x0:
				{
					resultString = &CharacterWasCreated;
					break;
				}

				case 0x1:
				{
					resultString = &InvalidNameSupplied;
					break;
				}

				case 0x2:
				{
					resultString = &CharacterAlreadyExists;
					break;
				}

				case 0x4:
				{
					resultString = &DatabaseTransactionFailed;
					break;
				}

				default: break;
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_CHEAT_CREATE_CHARACTER_RESULT,
				.size = sizeof(u8) + static_cast<u16>(resultString->size()) + 1
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->PutU8(result);
			buffer->PutString(*resultString);

			return true;
		}

		bool BuildCharacterDeleteResultMessage(std::shared_ptr<Bytebuffer>& buffer, const std::string& name, u8 result)
		{
			if (buffer == nullptr)
			{
				buffer = Bytebuffer::Borrow<sizeof(Network::PacketHeader) + 55>();
			}

			static const std::string CharacterWasDeleted = "Character was deleted";
			static const std::string CharacterDoesNotExists = "No character with the given name exists";
			static const std::string DatabaseTransactionFailed = "A database error occured during character deletion";
			static const std::string InsufficientPermission = "Could not delete character, insufficient permissions";
			static const std::string EmptyStr = "";

			const std::string* resultString = &EmptyStr;
			switch (result)
			{
				case 0x0:
				{
					resultString = &CharacterWasDeleted;
					break;
				}

				case 0x1:
				{
					resultString = &CharacterDoesNotExists;
					break;
				}

				case 0x2:
				{
					resultString = &DatabaseTransactionFailed;
					break;
				}

				case 0x4:
				{
					resultString = &InsufficientPermission;
					break;
				}

				default: break;
			}

			Network::PacketHeader header =
			{
				.opcode = Network::Opcode::SMSG_CHEAT_DELETE_CHARACTER_RESULT,
				.size = sizeof(u8) + static_cast<u16>(resultString->size()) + 1
			};

			if (buffer->GetSpace() < sizeof(Network::PacketHeader) + header.size)
				return false;

			buffer->Put(header);
			buffer->PutU8(result);
			buffer->PutString(*resultString);

			return true;
		}
	}
}