#include "aoe/legacy_recorded_game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <zlib.h>

#include "aoe/legacy_scenario.hpp"

namespace aoe {
namespace {

constexpr std::size_t max_file_size = 512U * 1024U * 1024U;
constexpr std::size_t max_header_size = 256U * 1024U * 1024U;
constexpr std::size_t max_action_size = 1U * 1024U * 1024U;
constexpr std::size_t max_chat_size = 64U * 1024U;
constexpr std::size_t max_records = 5U * 1024U * 1024U;

std::uint32_t u32(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::runtime_error("truncated recorded-game integer");
    }
    const auto* data = reinterpret_cast<const unsigned char*>(
        bytes.data() + offset
    );
    return static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8U) |
        (static_cast<std::uint32_t>(data[2]) << 16U) |
        (static_cast<std::uint32_t>(data[3]) << 24U);
}

std::int32_t i32(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int32_t>(u32(bytes, offset));
}

std::uint16_t u16(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::runtime_error("truncated recorded-game integer");
    }
    return std::to_integer<std::uint8_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(
             std::to_integer<std::uint8_t>(bytes[offset + 1])
         ) << 8U);
}

std::int16_t i16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int16_t>(u16(bytes, offset));
}

float f32(std::span<const std::byte> bytes, std::size_t offset) {
    const auto bits = u32(bytes, offset);
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    if (!std::isfinite(value)) {
        throw std::runtime_error("non-finite recorded-game float");
    }
    return value;
}

std::vector<std::byte> inflate_raw(std::span<const std::byte> compressed) {
    if (compressed.empty() ||
        compressed.size() > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("missing or oversized recorded header");
    }
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<std::byte*>(compressed.data())
    );
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw std::runtime_error("cannot initialize header DEFLATE decoder");
    }
    struct End {
        z_stream* stream;
        ~End() { inflateEnd(stream); }
    } end{&stream};
    std::vector<std::byte> output;
    std::array<std::byte, 64U * 1024U> chunk{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const auto produced = chunk.size() - stream.avail_out;
        if (output.size() > max_header_size - produced) {
            throw std::runtime_error("inflated header exceeds 256 MiB limit");
        }
        output.insert(output.end(), chunk.begin(), chunk.begin() + produced);
    }
    if (status != Z_STREAM_END || stream.avail_in != 0) {
        throw std::runtime_error("invalid raw-DEFLATE recorded header");
    }
    return output;
}

std::vector<std::byte> read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open recorded game");
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > max_file_size) {
        throw std::runtime_error("recorded game exceeds 512 MiB limit");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size())
        );
    }
    if (!input) throw std::runtime_error("cannot read complete recorded game");
    return bytes;
}

LegacyRecordedAction decode_action(std::span<const std::byte> bytes) {
    LegacyRecordedAction action;
    action.raw.assign(bytes.begin(), bytes.end());
    if (bytes.empty()) {
        action.diagnostic = "empty action";
        return action;
    }
    action.tag = std::to_integer<std::uint8_t>(bytes[0]);
    const auto byte = [&](std::size_t offset) {
        if (offset >= bytes.size()) {
            throw std::runtime_error("truncated action");
        }
        return std::to_integer<std::uint8_t>(bytes[offset]);
    };
    try {
        switch (action.tag) {
        case 0x00: {
            action.kind = LegacyRecordedActionKind::primary_action;
            if (bytes.size() < 20 || i16(bytes, 2) != 0 ||
                byte(9) != 0 || byte(10) != 0 || byte(11) != 0) {
                throw std::runtime_error(
                    "invalid primary-action constants or length"
                );
            }
            action.player_number = byte(1);
            action.target_id = i32(bytes, 4);
            const auto count = byte(8);
            action.x = f32(bytes, 12);
            action.y = f32(bytes, 16);
            if (count == 0xff) {
                if (bytes.size() != 20) {
                    throw std::runtime_error(
                        "primary-action selection-reuse has trailing bytes"
                    );
                }
                action.reuses_previous_selection = true;
            } else {
                if (bytes.size() !=
                    20U + static_cast<std::size_t>(count) * 4U) {
                    throw std::runtime_error(
                        "invalid primary-action selection count"
                    );
                }
                for (std::size_t index = 0; index < count; ++index) {
                    action.entity_ids.push_back(
                        i32(bytes, 20U + index * 4U)
                    );
                }
            }
            break;
        }
        case 0x03: {
            action.kind = LegacyRecordedActionKind::move;
            if (bytes.size() < 20 || i16(bytes, 2) != 0 ||
                i32(bytes, 4) != -1) {
                throw std::runtime_error("invalid move constants or length");
            }
            action.player_number = byte(1);
            const auto count = i32(bytes, 8);
            action.x = f32(bytes, 12);
            action.y = f32(bytes, 16);
            if (count == 0xff) {
                if (bytes.size() != 20) {
                    throw std::runtime_error(
                        "move selection-reuse has trailing bytes"
                    );
                }
                action.reuses_previous_selection = true;
            } else {
                if (count < 0 || count > 0xfe ||
                    bytes.size() != 20U + static_cast<std::size_t>(count) * 4U) {
                    throw std::runtime_error("invalid move selection count");
                }
                for (int index = 0; index < count; ++index) {
                    action.entity_ids.push_back(i32(bytes, 20U + index * 4U));
                }
            }
            break;
        }
        case 0x01: {
            action.kind = LegacyRecordedActionKind::stop;
            const auto count = byte(1);
            if (bytes.size() != 2U + static_cast<std::size_t>(count) * 4U) {
                throw std::runtime_error("invalid stop selection count");
            }
            for (std::size_t index = 0; index < count; ++index) {
                action.entity_ids.push_back(i32(bytes, 2U + index * 4U));
            }
            break;
        }
        case 0x77:
            action.kind = LegacyRecordedActionKind::train;
            if (bytes.size() != 12 || byte(1) != 0 || byte(2) != 0 ||
                byte(3) != 0) {
                throw std::runtime_error("invalid train constants or length");
            }
            action.building_id = i32(bytes, 4);
            action.commercial_unit_id = i16(bytes, 8);
            action.train_count = i16(bytes, 10);
            if (*action.train_count <= 0) {
                throw std::runtime_error("invalid train count");
            }
            break;
        case 0x65:
            action.kind = LegacyRecordedActionKind::research;
            if (bytes.size() != 16 || byte(1) != 0 || byte(2) != 0 ||
                byte(3) != 0 || byte(9) != 0 || i32(bytes, 12) != -1) {
                throw std::runtime_error(
                    "invalid research constants or length"
                );
            }
            action.building_id = i32(bytes, 4);
            action.player_number = byte(8);
            action.commercial_technology_id = i16(bytes, 10);
            break;
        case 0x6c:
            action.kind = LegacyRecordedActionKind::tribute;
            if (bytes.size() != 12) {
                throw std::runtime_error("invalid tribute length");
            }
            action.player_number = byte(1);
            action.target_player_number = byte(2);
            action.resource_type = byte(3);
            action.amount = f32(bytes, 4);
            action.transaction_fee = f32(bytes, 8);
            break;
        case 0x67: {
            action.kind = LegacyRecordedActionKind::diplomacy;
            if (bytes.size() != 16 || byte(3) != 0 ||
                byte(5) != 0 || byte(6) != 0 || byte(7) != 0 ||
                byte(13) != 0 || byte(14) != 0 || byte(15) != 0) {
                throw std::runtime_error(
                    "invalid multipurpose constants or length"
                );
            }
            action.action_type = byte(1);
            action.player_number = byte(2);
            action.option = byte(4);
            action.option2 = f32(bytes, 8);
            action.diplomatic_stance = byte(12);
            if (*action.action_type != 0) {
                throw std::runtime_error(
                    "multipurpose action is not diplomacy"
                );
            }
            if ((*action.diplomatic_stance != 0 &&
                 *action.diplomatic_stance != 1 &&
                 *action.diplomatic_stance != 3) ||
                *action.option2 !=
                    static_cast<float>(*action.diplomatic_stance)) {
                throw std::runtime_error(
                    "invalid diplomacy stance encoding"
                );
            }
            action.target_player_number = action.option;
            break;
        }
        case 0x0b:
            action.kind = LegacyRecordedActionKind::resign;
            if (bytes.size() != 7) {
                throw std::runtime_error(
                    "resign does not match proved seven-byte schema"
                );
            }
            action.player_number = byte(1);
            action.player_id = byte(2);
            action.disconnect = i32(bytes, 3);
            break;
        default:
            action.diagnostic = "unsupported action tag";
            return action;
        }
        action.schema_valid = true;
        action.diagnostic = "decoded from commit-pinned validated schema";
    } catch (const std::exception& error) {
        action.schema_valid = false;
        action.diagnostic = error.what();
    }
    return action;
}

std::string action_label(LegacyRecordedActionKind kind) {
    switch (kind) {
    case LegacyRecordedActionKind::primary_action:
        return "primary action";
    case LegacyRecordedActionKind::move: return "move";
    case LegacyRecordedActionKind::stop: return "stop";
    case LegacyRecordedActionKind::train: return "train";
    case LegacyRecordedActionKind::research: return "research";
    case LegacyRecordedActionKind::tribute: return "tribute";
    case LegacyRecordedActionKind::diplomacy: return "diplomacy";
    case LegacyRecordedActionKind::resign: return "resign";
    case LegacyRecordedActionKind::unsupported: return "unsupported";
    }
    return "unsupported";
}

}  // namespace

LegacyRecordedGameImportResult inspect_legacy_recorded_game_bytes(
    std::span<const std::byte> bytes
) {
    LegacyRecordedGameImportResult result;
    try {
        if (bytes.size() < 9) {
            throw std::runtime_error("truncated recorded-game envelope");
        }
        LegacyRecordedGameMetadata metadata;
        metadata.compressed_header_length = i32(bytes, 0);
        metadata.next_header = i32(bytes, 4);
        if (metadata.compressed_header_length <= 8 ||
            static_cast<std::size_t>(metadata.compressed_header_length) >
                bytes.size()) {
            throw std::runtime_error("invalid compressed-header length");
        }
        const std::size_t body_offset =
            static_cast<std::size_t>(metadata.compressed_header_length);
        metadata.compressed_header.assign(
            bytes.begin() + 8,
            bytes.begin() + body_offset
        );
        metadata.decompressed_header =
            inflate_raw(metadata.compressed_header);
        if (metadata.decompressed_header.size() < 8) {
            throw std::runtime_error("truncated recorded-game version");
        }
        const auto header =
            std::span<const std::byte>(metadata.decompressed_header);
        const auto* version =
            reinterpret_cast<const char*>(header.data());
        const auto* version_end = std::find(version, version + 8, '\0');
        metadata.game_version.assign(version, version_end);
        if (metadata.game_version.empty()) {
            throw std::runtime_error("empty recorded-game version");
        }

        std::size_t cursor = body_offset;
        while (cursor < bytes.size()) {
            if (metadata.records.size() >= max_records) {
                throw std::runtime_error("record count exceeds limit");
            }
            const std::size_t record_begin = cursor;
            const auto outer_tag = i32(bytes, cursor);
            cursor += 4;
            LegacyRecordedRecord record;
            record.file_offset = record_begin;
            if (outer_tag == 1) {
                const auto length = i32(bytes, cursor);
                cursor += 4;
                if (length < 1 ||
                    static_cast<std::size_t>(length) > max_action_size ||
                    cursor > bytes.size() ||
                    static_cast<std::size_t>(length) > bytes.size() - cursor ||
                    bytes.size() - cursor -
                        static_cast<std::size_t>(length) < 4) {
                    throw std::runtime_error("invalid recorded action length");
                }
                record.kind = LegacyRecordedRecordKind::action;
                record.action_tag =
                    std::to_integer<std::uint8_t>(bytes[cursor]);
                record.action = decode_action(bytes.subspan(
                    cursor,
                    static_cast<std::size_t>(length)
                ));
                cursor += static_cast<std::size_t>(length);
                record.execute_at = i32(bytes, cursor);
                cursor += 4;
            } else if (outer_tag == 2) {
                record.kind = LegacyRecordedRecordKind::synchronization;
                record.synchronization_interval = i32(bytes, cursor);
                cursor += 4;
                const auto unknown = i32(bytes, cursor);
                cursor += 4;
                if (unknown == 0) cursor += 7 * 4;
                if (cursor > bytes.size() || bytes.size() - cursor < 12) {
                    throw std::runtime_error(
                        "truncated synchronization record"
                    );
                }
                f32(bytes, cursor);
                f32(bytes, cursor + 4);
                i32(bytes, cursor + 8);
                cursor += 12;
            } else if (outer_tag == 4) {
                const auto message_tag = i32(bytes, cursor);
                cursor += 4;
                record.message_tag = message_tag;
                if (message_tag == 500) {
                    record.kind = LegacyRecordedRecordKind::game_start;
                    if (cursor > bytes.size() || bytes.size() - cursor < 20) {
                        throw std::runtime_error("truncated game-start record");
                    }
                    cursor += 20;
                } else if (message_tag == -1) {
                    record.kind = LegacyRecordedRecordKind::chat;
                    const auto length = i32(bytes, cursor);
                    cursor += 4;
                    if (length < 0 ||
                        static_cast<std::size_t>(length) > max_chat_size ||
                        cursor > bytes.size() ||
                        static_cast<std::size_t>(length) >
                            bytes.size() - cursor) {
                        throw std::runtime_error("invalid chat length");
                    }
                    if (length != 0) {
                        const auto* data = reinterpret_cast<const char*>(
                            bytes.data() + cursor
                        );
                        const std::size_t text_length =
                            data[length - 1] == '\0' ? length - 1 : length;
                        record.chat.assign(data, data + text_length);
                    }
                    cursor += static_cast<std::size_t>(length);
                } else {
                    cursor = record_begin;
                    break;
                }
            } else {
                cursor = record_begin;
                break;
            }
            record.raw.assign(
                bytes.begin() + record_begin,
                bytes.begin() + cursor
            );
            metadata.records.push_back(std::move(record));
        }
        metadata.unsupported_tail_offset = cursor;
        metadata.unsupported_tail.assign(bytes.begin() + cursor, bytes.end());
        result.status = LegacyRecordedGameStatus::inspected;
        result.metadata = std::move(metadata);
        result.diagnostic =
            "MGX header prefix and proved body records inspected; unknown "
            "record tail preserved without replay or save-game conversion";
        return result;
    } catch (const std::exception& error) {
        result.status = LegacyRecordedGameStatus::malformed;
        result.diagnostic = error.what();
        return result;
    }
}

LegacyRecordedGameImportResult inspect_legacy_recorded_game(
    const std::filesystem::path& path
) {
    try {
        const auto bytes = read_file(path);
        return inspect_legacy_recorded_game_bytes(bytes);
    } catch (const std::exception& error) {
        LegacyRecordedGameImportResult result;
        result.status = LegacyRecordedGameStatus::io_error;
        result.diagnostic = error.what();
        return result;
    }
}

LegacyRecordedReplayReport convert_legacy_recorded_game_to_replay(
    const LegacyRecordedGameMetadata& metadata,
    const LegacyRecordedReplayMappings& mappings
) {
    LegacyRecordedReplayReport report;
    Replay replay;
    bool convertible = metadata.unsupported_tail.empty();
    if (!metadata.unsupported_tail.empty()) {
        report.blockers.push_back(
            "unsupported body tail prevents a complete action stream"
        );
    }
    for (const auto& record : metadata.records) {
        if (record.kind != LegacyRecordedRecordKind::action ||
            !record.action.has_value()) {
            continue;
        }
        const auto& action = *record.action;
        report.actions.push_back(action);
        report.timeline.push_back({
            record.file_offset,
            record.execute_at.value_or(0),
            action,
        });
        if (record.execute_at) {
            report.required_mappings.execute_at_values.insert(
                *record.execute_at
            );
        }
        if (action.schema_valid &&
            action.kind == LegacyRecordedActionKind::primary_action) {
            report.required_mappings.primary_action_offsets.insert(
                record.file_offset
            );
        }
        if (action.schema_valid) {
            if (action.player_number) {
                report.required_mappings.player_numbers.insert(
                    *action.player_number
                );
            }
            if (action.player_id) {
                report.required_mappings.player_numbers.insert(
                    *action.player_id
                );
            }
            if (action.target_player_number) {
                report.required_mappings.player_numbers.insert(
                    *action.target_player_number
                );
            }
            report.required_mappings.entity_ids.insert(
                action.entity_ids.begin(), action.entity_ids.end()
            );
            if (action.target_id) {
                report.required_mappings.entity_ids.insert(
                    *action.target_id
                );
            }
            if (action.building_id) {
                report.required_mappings.entity_ids.insert(
                    *action.building_id
                );
            }
            if (action.commercial_unit_id) {
                report.required_mappings.commercial_unit_ids.insert(
                    *action.commercial_unit_id
                );
            }
            if (action.commercial_technology_id) {
                report.required_mappings.commercial_technology_ids.insert(
                    *action.commercial_technology_id
                );
            }
        }
        if (!action.schema_valid) {
            ++report.unsupported_action_count;
            ++report.unsupported_tags[action.tag];
            report.blockers.push_back(
                "action tag " + std::to_string(action.tag) + ": " +
                action.diagnostic
            );
            convertible = false;
            continue;
        }
        ++report.decoded_action_count;
        const auto tick = record.execute_at
            ? mappings.ticks.find(*record.execute_at)
            : mappings.ticks.end();
        if (tick == mappings.ticks.end()) {
            report.blockers.push_back(
                action_label(action.kind) +
                ": no explicit execute_at-to-Replay tick mapping"
            );
            convertible = false;
            continue;
        }
        std::vector<GameCommand> commands;
        const auto entity = [&](std::int32_t id) -> std::optional<EntityId> {
            const auto found = mappings.entities.find(id);
            if (found == mappings.entities.end()) return std::nullopt;
            return found->second;
        };
        switch (action.kind) {
        case LegacyRecordedActionKind::primary_action: {
            const auto context =
                mappings.primary_action_contexts.find(record.file_offset);
            if (context == mappings.primary_action_contexts.end()) {
                report.blockers.push_back(
                    "primary action: missing explicit target context for "
                    "record offset " + std::to_string(record.file_offset)
                );
                convertible = false;
                break;
            }
            if (action.reuses_previous_selection) {
                report.blockers.push_back(
                    "primary action: previous-selection reuse is not "
                    "represented"
                );
                convertible = false;
                break;
            }
            if (action.entity_ids.empty()) {
                report.blockers.push_back(
                    "primary action: empty selection has no exact native "
                    "action representation"
                );
                convertible = false;
                break;
            }
            if (mappings.players.find(*action.player_number) ==
                mappings.players.end()) {
                report.blockers.push_back(
                    "primary action: missing commercial-player mapping"
                );
                convertible = false;
                break;
            }
            const auto target = entity(*action.target_id);
            if (!target) {
                report.blockers.push_back(
                    "primary action: missing target entity mapping for " +
                    std::to_string(*action.target_id)
                );
                convertible = false;
                break;
            }
            if (context->second == LegacyPrimaryActionContext::attack ||
                context->second == LegacyPrimaryActionContext::repair) {
                report.blockers.push_back(
                    context->second == LegacyPrimaryActionContext::attack
                        ? "primary attack: targeted attack has no exact "
                          "GameCommand representation"
                        : "primary repair: repair has no exact GameCommand "
                          "representation"
                );
                convertible = false;
                break;
            }
            for (const auto id : action.entity_ids) {
                const auto actor = entity(id);
                if (!actor) {
                    report.blockers.push_back(
                        "primary action: missing selected entity mapping "
                        "for " + std::to_string(id)
                    );
                    convertible = false;
                    commands.clear();
                    break;
                }
                switch (context->second) {
                case LegacyPrimaryActionContext::gather_herdable:
                    commands.emplace_back(GatherUnitCommand{
                        *actor, *target
                    });
                    break;
                case LegacyPrimaryActionContext::convert:
                    commands.emplace_back(ConvertUnitCommand{
                        *actor, *target
                    });
                    break;
                case LegacyPrimaryActionContext::heal:
                    commands.emplace_back(HealUnitCommand{
                        *actor, *target
                    });
                    break;
                case LegacyPrimaryActionContext::collect_relic:
                    commands.emplace_back(CollectRelicCommand{
                        *actor, *target
                    });
                    break;
                case LegacyPrimaryActionContext::embark:
                    commands.emplace_back(EmbarkCommand{
                        *actor, *target
                    });
                    break;
                case LegacyPrimaryActionContext::attack:
                case LegacyPrimaryActionContext::repair:
                    break;
                }
            }
            break;
        }
        case LegacyRecordedActionKind::stop:
            for (const auto id : action.entity_ids) {
                const auto mapped = entity(id);
                if (!mapped) {
                    report.blockers.push_back(
                        "stop: missing entity mapping for " +
                        std::to_string(id)
                    );
                    convertible = false;
                    commands.clear();
                    break;
                }
                commands.emplace_back(StopUnitCommand{*mapped});
            }
            break;
        case LegacyRecordedActionKind::resign: {
            if (*action.disconnect != 0) {
                report.blockers.push_back(
                    "resign: disconnect flag has no Replay representation"
                );
                convertible = false;
                break;
            }
            if (action.player_number != action.player_id) {
                report.blockers.push_back(
                    "resign: distinct player-number/player-id mapping "
                    "cannot be represented"
                );
                convertible = false;
                break;
            }
            const auto player = mappings.players.find(*action.player_number);
            if (player == mappings.players.end()) {
                report.blockers.push_back(
                    "resign: missing commercial-player mapping"
                );
                convertible = false;
                break;
            }
            commands.emplace_back(ResignCommand{player->second});
            break;
        }
        case LegacyRecordedActionKind::move:
            if (action.reuses_previous_selection) {
                report.blockers.push_back(
                    "move: previous-selection reuse is not represented"
                );
                convertible = false;
                break;
            }
            if (mappings.players.find(*action.player_number) ==
                mappings.players.end()) {
                report.blockers.push_back(
                    "move: missing commercial-player mapping"
                );
                convertible = false;
                break;
            }
            if (std::trunc(*action.x) != *action.x ||
                std::trunc(*action.y) != *action.y ||
                static_cast<double>(*action.x) <
                    std::numeric_limits<int>::min() ||
                static_cast<double>(*action.x) >
                    std::numeric_limits<int>::max() ||
                static_cast<double>(*action.y) <
                    std::numeric_limits<int>::min() ||
                static_cast<double>(*action.y) >
                    std::numeric_limits<int>::max()) {
                report.blockers.push_back(
                    "move: non-integral or out-of-range coordinate"
                );
                convertible = false;
                break;
            }
            for (const auto id : action.entity_ids) {
                const auto mapped = entity(id);
                if (!mapped) {
                    report.blockers.push_back(
                        "move: missing entity mapping for " +
                        std::to_string(id)
                    );
                    convertible = false;
                    commands.clear();
                    break;
                }
                commands.emplace_back(MoveUnitCommand{
                    *mapped,
                    {static_cast<int>(*action.x),
                     static_cast<int>(*action.y)},
                });
            }
            break;
        case LegacyRecordedActionKind::train: {
            const auto building = entity(*action.building_id);
            const auto unit =
                mappings.units.find(*action.commercial_unit_id);
            if (!building || unit == mappings.units.end()) {
                report.blockers.push_back(
                    "train: missing building or commercial-unit mapping"
                );
                convertible = false;
                break;
            }
            for (int count = 0; count < *action.train_count; ++count) {
                commands.emplace_back(
                    QueueUnitCommand{*building, unit->second}
                );
            }
            break;
        }
        case LegacyRecordedActionKind::research: {
            const auto building = entity(*action.building_id);
            const auto technology = mappings.technologies.find(
                *action.commercial_technology_id
            );
            if (!building ||
                technology == mappings.technologies.end() ||
                mappings.players.find(*action.player_number) ==
                    mappings.players.end()) {
                report.blockers.push_back(
                    "research: missing building, player, or technology "
                    "mapping"
                );
                convertible = false;
                break;
            }
            commands.emplace_back(ResearchTechnologyCommand{
                *building, technology->second
            });
            break;
        }
        case LegacyRecordedActionKind::tribute:
            report.blockers.push_back(
                "tribute: float amount and transaction fee are not both "
                "represented by GameCommand"
            );
            convertible = false;
            break;
        case LegacyRecordedActionKind::diplomacy: {
            const auto source =
                mappings.players.find(*action.player_number);
            const auto target =
                mappings.players.find(*action.target_player_number);
            if (source == mappings.players.end() ||
                target == mappings.players.end()) {
                report.blockers.push_back(
                    "diplomacy: missing commercial-player mapping"
                );
                convertible = false;
                break;
            }
            const Diplomacy relation =
                *action.diplomatic_stance == 0 ? Diplomacy::ally :
                *action.diplomatic_stance == 1 ? Diplomacy::neutral :
                Diplomacy::enemy;
            commands.emplace_back(SetDiplomacyCommand{
                source->second, target->second, relation
            });
            break;
        }
        case LegacyRecordedActionKind::unsupported:
            convertible = false;
            break;
        }
        if (commands.empty() &&
            action.kind != LegacyRecordedActionKind::stop) {
            continue;
        }
        if (convertible) {
            for (auto& command : commands) {
                replay.record(tick->second, std::move(command));
            }
        }
    }
    if (convertible) report.replay = std::move(replay);
    return report;
}

}  // namespace aoe
