#include "aoe/localization.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace aoe {
namespace {

std::map<std::string, std::string> english_strings() {
    return {
        {"campaign.title", "BOUNDED CAMPAIGN MANIFEST"},
        {"campaign.briefing", "CAMPAIGN MISSION BRIEFING"},
        {"campaign.debrief", "CAMPAIGN MISSION DEBRIEF"},
        {"browser.title", "SAVE / LOAD / REPLAY BROWSER"},
        {"browser.entry_one", "{count} ENTRY"},
        {"browser.entry_other", "{count} ENTRIES"},
        {"diplomacy.title", "DIPLOMACY AND TRIBUTE"},
        {"diplomacy.ally", "ALLY"},
        {"diplomacy.neutral", "NEUTRAL"},
        {"diplomacy.enemy", "ENEMY"},
        {"editor.title", "SCENARIO EDITOR"},
        {"editor.tabs",
         "TAB FOCUS: TERRAIN  PLAYERS  OBJECTIVES  TRIGGERS  FILE"},
        {"editor.tools",
         "1 GRASS  2 WATER  3 FOREST  E ELEVATION  U UNIT  B BUILDING  X ERASE"},
        {"editor.actions",
         "ARROWS CURSOR ENTER APPLY  P PLAYER A AGE C CIV [/] RES D DIPLOMACY R RULE"},
        {"editor.files",
         "V VALIDATE  CTRL+O LOAD  CTRL+S SAVE  CTRL+Z/Y UNDO/REDO  ESC CLOSE"},
        {"frontend.main.single_player", "Single Player"},
        {"frontend.main.multiplayer", "Multiplayer"},
        {"frontend.main.learn_to_play", "Learn to Play"},
        {"frontend.main.map_editor", "Map Editor"},
        {"frontend.main.history", "History"},
        {"frontend.main.options", "Options"},
        {"frontend.main.exit", "Exit"},
        {"hud.food", "FOOD"},
        {"hud.gold", "GOLD"},
        {"hud.idle", "IDLE"},
        {"hud.population", "POP"},
        {"hud.stone", "STONE"},
        {"hud.wood", "WOOD"},
        {"hud.hit_points", "HP {current}/{maximum}"},
        {"hud.status", "STATUS  {status}"},
        {"hud.garrison", "  GARRISON {count}"},
        {"hud.carry", "  CARRY {amount} {resource}"},
        {"hud.resource_value", "{resource} {amount}"},
        {"hud.population_value", "POP {population}/{capacity}"},
        {"hud.population_paused", "POP {population}/{capacity} PAUSED"},
        {"hud.active_mode", "MODE: {mode}  |  ESC CANCEL"},
        {"hud.mode_trade_route", "TRADE ROUTE"},
        {"hud.mode_conversion", "CONVERT TARGET"},
        {"hud.mode_guard", "GUARD TARGET"},
        {"hud.mode_attack_ground", "ATTACK GROUND"},
        {"hud.mode_patrol", "PATROL ENDPOINT"},
        {"hud.mode_attack_move", "ATTACK MOVE"},
        {"hud.mode_build", "BUILD PLACEMENT"},
        {"hud.match_complete", "MATCH COMPLETE: {outcome}"},
        {"ai.debug_summary",
         "AI DEBUG  {difficulty}  Phase {phase}  Age goal {age}  Objective {objective}"},
        {"ai.debug_forces",
         "Workers W/F/G/S {wood}/{food}/{gold}/{stone}  Army M/R/C/S/N {melee}/{ranged}/{cavalry}/{siege}/{naval}"},
        {"ai.debug_position",
         "Desired {counter}  Stance {stance}  H {home_x},{home_y}  R {rally_x},{rally_y}{target}"},
        {"ai.debug_target", "  T {x},{y}"},
        {"ai.stance_retreat", "retreat"},
        {"ai.stance_advance", "advance"},
        {"world.garrison_badge", "G{count}"},
        {"world.volley_badge", "V{count}"},
        {"world.garrison_volley_badge", "G{garrison} V{volley}"},
        {"technology.command_cost", "{name} ({cost})"},
        {"diplomacy.market_rates",
         "MARKET BUY/SELL  F {food_buy}/{food_sell}  W {wood_buy}/{wood_sell}  S {stone_buy}/{stone_sell}"},
        {"control_group.units_one", "Group {group}: {count} unit"},
        {"control_group.units_other", "Group {group}: {count} units"},
        {"control_group.building", "Group {group}: building #{building}"},
        {"control_group.cleared", "Group {group}: cleared"},
        {"control_group.centered", "Group {group} centered"},
        {"control_group.recalled", "Group {group} recalled"},
        {"selection.idle_villager", "Idle villager {index}/{count}"},
        {"selection.idle_military", "Idle military {index}/{count}"},
        {"multiplayer.title", "BOUNDED LOCKSTEP SESSION"},
        {"objective.done", "DONE"},
        {"objective.failed", "FAILED"},
        {"objective.optional", "OPTIONAL"},
        {"objective.required", "REQUIRED"},
        {"statistics.title", "MATCH STATISTICS"},
        {"statistics.economy", "ECONOMY"},
        {"statistics.military", "MILITARY"},
        {"statistics.society", "SOCIETY"},
        {"statistics.technology", "TECHNOLOGY"},
        {"statistics.timeline", "TIMELINE"},
        {"statistics.unavailable", "UNAVAILABLE"},
        {"statistics.continue", "CONTINUE"},
        {"statistics.rematch", "REMATCH"},
        {"statistics.back", "BACK TO MENU"},
        {"technology_tree.title", "CIVILIZATION TECHNOLOGY TREE"},
        {"technology_tree.title_civilization", "{title}: {civilization}"},
        {"technology_tree.detail",
         "{name}  COST W{wood} F{food} G{gold} S{stone}  {requirement}"},
        {"technology_tree.help",
         "Q/E CIV  ARROWS FOCUS  WASD PAN  TAB NEXT  +/- ZOOM  F9/ESC CLOSE"},
        {"technology_tree.dark_age", "DARK AGE"},
        {"technology_tree.feudal_age", "FEUDAL AGE"},
        {"technology_tree.castle_age", "CASTLE AGE"},
        {"technology_tree.imperial_age", "IMPERIAL AGE"},
        {"technology_tree.disabled", "DISABLED"},
        {"technology_tree.researched", "RESEARCHED"},
        {"technology_tree.available", "AVAILABLE"},
        {"technology_tree.missing_evidence",
         "MISSING EVIDENCE: ORIGINAL NODE ICONS, FRAMING, AND FONT METRICS"},
        {"ui.message", "MESSAGE"},
        {"ui.objectives", "OBJECTIVES"},
        {"ui.tab_close", "TAB: CLOSE"},
    };
}

std::string normalize_locale(std::string_view raw) {
    if (raw.empty()) return "en";
    if (raw.size() > 35) {
        throw std::invalid_argument("locale identifier is too long");
    }
    std::string result;
    result.reserve(raw.size());
    bool prior_separator = true;
    for (const unsigned char byte : raw) {
        if (std::isalnum(byte)) {
            result.push_back(static_cast<char>(std::tolower(byte)));
            prior_separator = false;
        } else if ((byte == '-' || byte == '_') && !prior_separator) {
            result.push_back('-');
            prior_separator = true;
        } else {
            throw std::invalid_argument("invalid locale identifier");
        }
    }
    if (prior_separator || result.size() < 2) {
        throw std::invalid_argument("invalid locale identifier");
    }
    return result;
}

std::optional<std::uint32_t> next_utf8(
    std::string_view text,
    std::size_t& offset
) noexcept {
    if (offset >= text.size()) return std::nullopt;
    const auto byte = static_cast<unsigned char>(text[offset++]);
    if (byte < 0x80) return byte;
    int continuation_count{};
    std::uint32_t code{};
    std::uint32_t minimum{};
    if ((byte & 0xe0) == 0xc0) {
        continuation_count = 1;
        code = byte & 0x1f;
        minimum = 0x80;
    } else if ((byte & 0xf0) == 0xe0) {
        continuation_count = 2;
        code = byte & 0x0f;
        minimum = 0x800;
    } else if ((byte & 0xf8) == 0xf0) {
        continuation_count = 3;
        code = byte & 0x07;
        minimum = 0x10000;
    } else {
        offset = text.size() + 1;
        return std::nullopt;
    }
    for (int index = 0; index < continuation_count; ++index) {
        if (offset >= text.size()) {
            offset = text.size() + 1;
            return std::nullopt;
        }
        const auto continuation =
            static_cast<unsigned char>(text[offset++]);
        if ((continuation & 0xc0) != 0x80) {
            offset = text.size() + 1;
            return std::nullopt;
        }
        code = (code << 6) | (continuation & 0x3f);
    }
    if (code < minimum || code > 0x10ffff ||
        (code >= 0xd800 && code <= 0xdfff)) {
        offset = text.size() + 1;
        return std::nullopt;
    }
    return code;
}

bool valid_value(std::string_view value) {
    if (value.empty() || value.size() > 512 || !valid_utf8(value)) {
        return false;
    }
    std::size_t offset{};
    while (offset < value.size()) {
        const auto code = next_utf8(value, offset);
        if (!code || *code < 32 || *code == 127) return false;
    }
    return true;
}

bool locale_matches(
    std::string_view requested,
    std::string_view available
) {
    if (requested == available) return true;
    const auto language = [](std::string_view locale) {
        const std::size_t separator = locale.find('-');
        return locale.substr(0, separator);
    };
    return language(requested) == language(available);
}

std::string primary_language(std::string_view locale) {
    const std::string normalized = normalize_locale(locale);
    const auto separator = normalized.find('-');
    return normalized.substr(0, separator);
}

StringTable load_external_table(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open language file");

    std::string line;
    int line_number{};
    bool header{};
    bool saw_locale{};
    std::string locale;
    std::map<std::string, std::string> translated = english_strings();
    std::set<std::string> overridden;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line.starts_with('#')) continue;
        std::istringstream record(line);
        std::string keyword;
        record >> keyword;
        if (!header) {
            int version{};
            record >> version;
            if (keyword != "aoe-language" || version != 1) {
                throw std::runtime_error("unsupported language file header");
            }
            header = true;
        } else if (keyword == "locale" && !saw_locale) {
            std::string raw;
            record >> std::quoted(raw);
            locale = normalize_locale(raw);
            saw_locale = true;
        } else if (keyword == "string" && saw_locale) {
            std::string key;
            std::string value;
            record >> std::quoted(key) >> std::quoted(value);
            if (!translated.contains(key)) {
                throw std::runtime_error(
                    "unknown language key at line " +
                    std::to_string(line_number)
                );
            }
            if (!overridden.insert(key).second || !valid_value(value)) {
                throw std::runtime_error(
                    "invalid language string at line " +
                    std::to_string(line_number)
                );
            }
            translated[key] = std::move(value);
        } else if (keyword == "literal" && saw_locale) {
            std::string english;
            std::string value;
            record >> std::quoted(english) >> std::quoted(value);
            const std::string key = stable_literal_key(english);
            if (!overridden.insert(key).second || !valid_value(english) ||
                !valid_value(value)) {
                throw std::runtime_error(
                    "invalid language literal at line " +
                    std::to_string(line_number)
                );
            }
            translated.emplace(key, std::move(value));
        } else {
            throw std::runtime_error(
                "invalid language record at line " +
                std::to_string(line_number)
            );
        }
        if (!record) {
            throw std::runtime_error(
                "malformed language data at line " +
                std::to_string(line_number)
            );
        }
        record >> std::ws;
        if (record.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error(
                "trailing language data at line " +
                std::to_string(line_number)
            );
        }
    }
    if (!header || !saw_locale) {
        throw std::runtime_error("incomplete language file");
    }
    return {std::move(locale), std::move(translated)};
}

std::string utf16_to_utf8(
    const std::vector<std::uint16_t>& input
) {
    std::string output;
    for (std::size_t index = 0; index < input.size(); ++index) {
        std::uint32_t code = input[index];
        if (code >= 0xd800 && code <= 0xdbff) {
            if (++index >= input.size() ||
                input[index] < 0xdc00 || input[index] > 0xdfff) {
                throw std::runtime_error("invalid UTF-16 surrogate pair");
            }
            code = 0x10000 +
                ((code - 0xd800) << 10) + (input[index] - 0xdc00);
        } else if (code >= 0xdc00 && code <= 0xdfff) {
            throw std::runtime_error("invalid UTF-16 low surrogate");
        }
        if (code <= 0x7f) {
            output.push_back(static_cast<char>(code));
        } else if (code <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (code >> 6)));
            output.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code <= 0xffff) {
            output.push_back(static_cast<char>(0xe0 | (code >> 12)));
            output.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (code >> 18)));
            output.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }
    return output;
}

}  // namespace

std::map<std::uint32_t, std::string> extract_pe_string_resources(
    const std::filesystem::path& external_dll,
    std::uint16_t language_id
) {
    std::ifstream input(external_dll, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open external language DLL");
    const auto size = input.tellg();
    if (size <= 0 || size > 64 * 1024 * 1024) {
        throw std::runtime_error("invalid external language DLL size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    if (!input) throw std::runtime_error("truncated external language DLL");
    const auto u16 = [&bytes](std::size_t offset) {
        if (offset + 2 > bytes.size()) throw std::runtime_error("truncated PE");
        return static_cast<std::uint16_t>(
            bytes[offset] | (bytes[offset + 1] << 8)
        );
    };
    const auto u32 = [&bytes](std::size_t offset) {
        if (offset + 4 > bytes.size()) throw std::runtime_error("truncated PE");
        return static_cast<std::uint32_t>(
            bytes[offset] | (bytes[offset + 1] << 8) |
            (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24)
        );
    };
    if (u16(0) != 0x5a4d) throw std::runtime_error("not a PE image");
    const std::size_t pe = u32(0x3c);
    if (pe + 24 > bytes.size() || u32(pe) != 0x00004550) {
        throw std::runtime_error("invalid PE signature");
    }
    const std::uint16_t section_count = u16(pe + 6);
    const std::uint16_t optional_size = u16(pe + 20);
    const std::size_t optional = pe + 24;
    const std::uint16_t magic = u16(optional);
    const std::size_t directories =
        magic == 0x10b ? optional + 96 :
        magic == 0x20b ? optional + 112 :
        throw std::runtime_error("unsupported PE optional header");
    const std::uint32_t resource_rva = u32(directories + 16);
    const std::uint32_t resource_size = u32(directories + 20);
    if (resource_rva == 0 || resource_size < 16) {
        throw std::runtime_error("PE has no resource directory");
    }
    struct Section {
        std::uint32_t virtual_address;
        std::uint32_t virtual_size;
        std::uint32_t raw_offset;
        std::uint32_t raw_size;
    };
    std::vector<Section> sections;
    const std::size_t section_table = optional + optional_size;
    for (std::uint16_t index = 0; index < section_count; ++index) {
        const std::size_t offset = section_table + index * 40;
        sections.push_back({
            u32(offset + 12), u32(offset + 8),
            u32(offset + 20), u32(offset + 16),
        });
    }
    const auto rva_offset = [&sections, &bytes](std::uint32_t rva) {
        for (const Section& section : sections) {
            const std::uint32_t span =
                std::max(section.virtual_size, section.raw_size);
            if (rva >= section.virtual_address &&
                rva - section.virtual_address < span) {
                const std::size_t result =
                    section.raw_offset + (rva - section.virtual_address);
                if (result > bytes.size()) {
                    throw std::runtime_error("PE RVA outside file");
                }
                return result;
            }
        }
        throw std::runtime_error("unmapped PE RVA");
    };
    const std::size_t root = rva_offset(resource_rva);
    const auto directory_entries = [&u16, &u32, root, resource_size](
        std::uint32_t relative
    ) {
        if (relative + 16 > resource_size) {
            throw std::runtime_error("resource directory outside bounds");
        }
        const std::size_t directory = root + relative;
        const std::size_t count = u16(directory + 12) + u16(directory + 14);
        std::vector<std::pair<std::uint32_t, std::uint32_t>> result;
        for (std::size_t index = 0; index < count; ++index) {
            result.emplace_back(
                u32(directory + 16 + index * 8),
                u32(directory + 20 + index * 8)
            );
        }
        return result;
    };
    const auto child = [&directory_entries](
        std::uint32_t directory,
        std::uint16_t id
    ) {
        for (const auto [name, target] : directory_entries(directory)) {
            if ((name & 0x80000000U) == 0 &&
                static_cast<std::uint16_t>(name) == id) {
                return target;
            }
        }
        throw std::runtime_error("required PE resource ID absent");
    };
    constexpr std::uint16_t rt_string = 6;
    const std::uint32_t type_target = child(0, rt_string);
    if ((type_target & 0x80000000U) == 0) {
        throw std::runtime_error("invalid RT_STRING directory");
    }
    std::map<std::uint32_t, std::string> strings;
    for (const auto [block_name, block_target] :
         directory_entries(type_target & 0x7fffffffU)) {
        if ((block_name & 0x80000000U) != 0 ||
            (block_target & 0x80000000U) == 0) continue;
        const std::uint16_t block_id = static_cast<std::uint16_t>(block_name);
        std::uint32_t language_target{};
        try {
            language_target = child(
                block_target & 0x7fffffffU, language_id
            );
        } catch (const std::runtime_error&) {
            continue;
        }
        if ((language_target & 0x80000000U) != 0 ||
            language_target + 16 > resource_size) {
            throw std::runtime_error("invalid RT_STRING data entry");
        }
        const std::size_t data_entry = root + language_target;
        const std::uint32_t data_rva = u32(data_entry);
        const std::uint32_t data_size = u32(data_entry + 4);
        std::size_t cursor = rva_offset(data_rva);
        const std::size_t end = cursor + data_size;
        if (end > bytes.size()) throw std::runtime_error("truncated RT_STRING");
        for (std::uint32_t slot = 0; slot < 16; ++slot) {
            if (cursor + 2 > end) throw std::runtime_error("truncated string block");
            const std::uint16_t length = u16(cursor);
            cursor += 2;
            if (cursor + static_cast<std::size_t>(length) * 2 > end) {
                throw std::runtime_error("truncated UTF-16 string");
            }
            std::vector<std::uint16_t> value;
            value.reserve(length);
            for (std::uint16_t index = 0; index < length; ++index) {
                value.push_back(u16(cursor));
                cursor += 2;
            }
            if (length != 0) {
                strings[(static_cast<std::uint32_t>(block_id) - 1) * 16 +
                        slot] = utf16_to_utf8(value);
            }
        }
    }
    return strings;
}

LegacyLanguageReport load_legacy_language_sources(
    std::string_view locale,
    std::uint16_t language_id,
    const std::vector<std::filesystem::path>& external_dlls,
    const std::map<std::uint32_t, std::string>& id_to_key
) {
    if (external_dlls.empty()) {
        throw std::invalid_argument("no external language DLL sources");
    }
    std::map<std::uint32_t, std::string> extracted;
    for (const auto& path : external_dlls) {
        const auto source = extract_pe_string_resources(path, language_id);
        extracted.insert(source.begin(), source.end());
        for (const auto& [id, value] : source) extracted[id] = value;
    }
    auto translated = english_strings();
    std::map<std::uint32_t, std::string> unknown;
    for (const auto& [id, value] : extracted) {
        const auto mapping = id_to_key.find(id);
        if (mapping == id_to_key.end()) {
            unknown[id] = value;
            continue;
        }
        if (!translated.contains(mapping->second)) {
            throw std::runtime_error("legacy mapping names unknown StringTable key");
        }
        translated[mapping->second] = value;
    }
    const std::string normalized = normalize_locale(locale);
    auto fonts = extract_legacy_font_styles(extracted);
    return {
        StringTable(normalized, std::move(translated), extracted),
        std::move(extracted),
        std::move(unknown),
        external_dlls,
        language_id,
        std::move(fonts),
    };
}

StringTable::StringTable(
    std::string locale,
    std::map<std::string, std::string> strings,
    std::map<std::uint32_t, std::string> numeric_strings
) : locale_(std::move(locale)), strings_(std::move(strings)),
    numeric_strings_(std::move(numeric_strings)) {
    if (locale_.empty() || strings_.empty()) {
        throw std::invalid_argument("empty string table");
    }
    has_literal_overrides_ = std::ranges::any_of(
        strings_, [](const auto& entry) {
            return entry.first.starts_with("ui.literal.");
        }
    );
}

std::string_view StringTable::text(std::string_view key) const {
    const auto found = strings_.find(std::string{key});
    if (found == strings_.end()) {
        throw std::out_of_range("unknown localization key");
    }
    return found->second;
}

bool StringTable::contains(std::string_view key) const noexcept {
    return strings_.contains(std::string{key});
}

std::string StringTable::translate_literal(std::string_view english) const {
    const auto found = strings_.find(stable_literal_key(english));
    return found == strings_.end() ? std::string{english} : found->second;
}

std::string StringTable::text_or(
    std::uint32_t numeric_id,
    std::string_view fallback
) const {
    const auto found = numeric_strings_.find(numeric_id);
    return found == numeric_strings_.end()
        ? std::string{fallback} : found->second;
}

bool StringTable::contains(std::uint32_t numeric_id) const noexcept {
    return numeric_strings_.contains(numeric_id);
}

std::string stable_literal_key(std::string_view english) {
    // FNV-1a gives deterministic source-independent IDs. Collision safety is
    // enforced by language-file duplicate rejection and generated catalogs.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : english) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << "ui.literal." << std::hex << std::setfill('0')
           << std::setw(16) << hash;
    return output.str();
}

std::string fit_localized_text(
    std::string_view text,
    std::size_t maximum_codepoints,
    bool ellipsis
) {
    if (!valid_utf8(text)) throw std::invalid_argument("invalid UTF-8 text");
    if (maximum_codepoints == 0) return {};
    std::size_t offset{};
    std::size_t count{};
    while (offset < text.size() && count < maximum_codepoints) {
        static_cast<void>(next_utf8(text, offset));
        ++count;
    }
    if (offset == text.size()) return std::string{text};
    if (!ellipsis) return std::string{text.substr(0, offset)};
    if (maximum_codepoints == 1) return "\xe2\x80\xa6";
    offset = 0;
    for (std::size_t index = 0; index < maximum_codepoints - 1; ++index) {
        static_cast<void>(next_utf8(text, offset));
    }
    return std::string{text.substr(0, offset)} + "\xe2\x80\xa6";
}

const std::vector<LanguageProfile>& shipped_language_profiles() {
    // Classic/HD package directory aliases and Win32 LANGIDs. Payload remains
    // user-owned; these values are executable/resource semantics only.
    static const std::vector<LanguageProfile> profiles{
        {"en", 0x0409, {"en"}, "en", false},
        {"de", 0x0407, {"de"}, "de", false},
        {"fr", 0x040c, {"fr"}, "fr", false},
        {"es", 0x040a, {"es"}, "es", false},
        {"it", 0x0410, {"it"}, "it", false},
        {"pt", 0x0416, {"br", "pt"}, "br", false},
        {"nl", 0x0413, {"nl"}, "nl", false},
        {"ru", 0x0419, {"ru"}, "ru", false},
        {"ja", 0x0411, {"jp", "ja"}, "jp", false},
        {"ko", 0x0412, {"ko"}, "ko", false},
        {"zh", 0x0804, {"zh", "zh-cn"}, "zh", false},
    };
    return profiles;
}

const LanguageProfile& language_profile(std::string_view locale) {
    const std::string language = primary_language(locale);
    const auto& profiles = shipped_language_profiles();
    const auto found = std::ranges::find_if(
        profiles,
        [&language](const LanguageProfile& profile) {
            return profile.locale == language;
        }
    );
    if (found == profiles.end()) {
        throw std::invalid_argument("unsupported shipped locale");
    }
    return *found;
}

PluralCategory plural_category(
    std::string_view locale,
    std::int64_t count
) {
    const std::string language = primary_language(locale);
    const std::uint64_t magnitude = count < 0
        ? static_cast<std::uint64_t>(-(count + 1)) + 1
        : static_cast<std::uint64_t>(count);
    const auto mod10 = magnitude % 10;
    const auto mod100 = magnitude % 100;
    if (language == "ru") {
        if (mod10 == 1 && mod100 != 11) return PluralCategory::one;
        if (mod10 >= 2 && mod10 <= 4 &&
            (mod100 < 12 || mod100 > 14)) return PluralCategory::few;
        if (mod10 == 0 || mod10 >= 5 ||
            (mod100 >= 11 && mod100 <= 14)) return PluralCategory::many;
        return PluralCategory::other;
    }
    if (language == "fr" || language == "pt") {
        return magnitude == 0 || magnitude == 1
            ? PluralCategory::one : PluralCategory::other;
    }
    if (language == "ja" || language == "ko" || language == "zh") {
        return PluralCategory::other;
    }
    return magnitude == 1 ? PluralCategory::one : PluralCategory::other;
}

std::string format_localized(
    std::string_view pattern,
    const std::map<std::string, std::string>& arguments
) {
    std::string result{pattern};
    for (const auto& [name, value] : arguments) {
        if (name.empty() || name.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
            ) != std::string::npos) {
            throw std::invalid_argument("invalid localization argument name");
        }
        const std::string marker = "{" + name + "}";
        std::size_t offset{};
        while ((offset = result.find(marker, offset)) != std::string::npos) {
            result.replace(offset, marker.size(), value);
            offset += value.size();
        }
    }
    if (result.find('{') != std::string::npos ||
        result.find('}') != std::string::npos) {
        throw std::runtime_error("unresolved localization argument");
    }
    return result;
}

std::map<std::uint32_t, std::string> legacy_ui_string_catalog() {
    // IDs proven by supplied language resources and menu dispatch.
    return {
        {9500, "frontend.main.single_player"},
        {9501, "frontend.main.multiplayer"},
        {9503, "frontend.main.learn_to_play"},
        {9504, "frontend.main.map_editor"},
        {9505, "frontend.main.history"},
        {9506, "frontend.main.options"},
        {9509, "frontend.main.exit"},
    };
}

std::vector<std::filesystem::path> discover_legacy_language_sources(
    const std::filesystem::path& packaged_asset_root,
    std::string_view locale
) {
    const LanguageProfile& profile = language_profile(locale);
    constexpr std::array<std::string_view, 3> names{
        "language.dll", "language_x1.dll", "language_x1_p1.dll"
    };
    for (const std::string& directory : profile.archive_directories) {
        const auto root = packaged_asset_root / "Bin" / directory;
        std::vector<std::filesystem::path> found;
        for (const auto name : names) {
            const auto path = root / name;
            if (std::filesystem::is_regular_file(path)) found.push_back(path);
        }
        if (!found.empty()) return found;
    }
    return {};
}

std::vector<std::filesystem::path> localized_audio_directories(
    const std::filesystem::path& packaged_asset_root,
    std::string_view locale,
    std::string_view category
) {
    if (category != "scenario" && category != "campaign" &&
        category != "taunt") {
        throw std::invalid_argument("unknown localized audio category");
    }
    std::string selected{"en"};
    try { selected = language_profile(locale).audio_directory; }
    catch (const std::invalid_argument&) {
        const auto found = std::ranges::find_if(
            shipped_language_profiles(), [locale](const LanguageProfile& p) {
                return p.audio_directory == locale;
            }
        );
        if (found != shipped_language_profiles().end()) {
            selected = found->audio_directory;
        }
    }
    const std::filesystem::path family = category == "taunt"
        ? packaged_asset_root / "Taunt"
        : packaged_asset_root / "Sound" / std::filesystem::path{category};
    std::vector<std::filesystem::path> result{family / selected};
    if (selected != "en") result.push_back(family / "en");
    return result;
}

std::map<std::uint32_t, LegacyFontStyle> extract_legacy_font_styles(
    const std::map<std::uint32_t, std::string>& strings
) {
    static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 35>
        slots{{
            {0,110},{1,113},{2,134},{3,137},{4,116},{6,131},{7,119},
            {8,128},{9,122},{10,125},{11,128},{12,140},{13,143},
            {14,146},{15,149},{16,152},{17,155},{18,158},{19,161},
            {20,164},{21,167},{22,170},{23,173},{24,176},{25,179},
            {26,182},{27,185},{28,188},{29,191},{30,194},{31,197},
            {32,200},{33,203},{34,206},{35,209}
        }};
    std::map<std::uint32_t, LegacyFontStyle> result;
    for (const auto [slot, base] : slots) {
        const auto family = strings.find(base);
        const auto height = strings.find(base + 1);
        const auto style = strings.find(base + 2);
        if (family == strings.end() || height == strings.end() ||
            style == strings.end()) continue;
        int parsed{};
        const auto [end, error] = std::from_chars(
            height->second.data(),
            height->second.data() + height->second.size(), parsed
        );
        if (error != std::errc{} || end != height->second.data() +
                height->second.size() || parsed <= 0 || parsed > 256) {
            throw std::runtime_error("invalid localized font height");
        }
        LegacyFontStyle font{family->second, parsed, false, false};
        for (const unsigned char c : style->second) {
            if (c == 'B' || c == 'b') font.bold = true;
            if (c == 'I' || c == 'i') font.italic = true;
        }
        result.emplace(slot, std::move(font));
    }
    return result;
}

std::string StringTable::count_text(
    std::string_view singular_key,
    std::string_view plural_key,
    std::int64_t count,
    const std::map<std::string, std::string>& arguments
) const {
    std::map<std::string, std::string> values = arguments;
    values["count"] = std::to_string(count);
    return format_localized(
        text(plural_category(locale_, count) == PluralCategory::one
            ? singular_key : plural_key),
        values
    );
}

std::string StringTable::format(
    std::string_view key,
    const std::map<std::string, std::string>& arguments
) const {
    return format_localized(text(key), arguments);
}

bool valid_utf8(std::string_view text) noexcept {
    std::size_t offset{};
    while (offset < text.size()) {
        const std::size_t prior = offset;
        if (!next_utf8(text, offset) || offset <= prior ||
            offset > text.size()) {
            return false;
        }
    }
    return true;
}

std::string debug_font_fallback(std::string_view text) {
    if (!valid_utf8(text)) {
        throw std::invalid_argument("invalid UTF-8 text");
    }
    std::string result;
    result.reserve(text.size());
    std::size_t offset{};
    while (offset < text.size()) {
        const std::uint32_t code = *next_utf8(text, offset);
        if (code >= 32 && code <= 126) {
            result.push_back(static_cast<char>(code));
            continue;
        }
        const char folded =
            code == 0x00c0 || code == 0x00c1 || code == 0x00c2 ||
            code == 0x00c3 || code == 0x00c4 || code == 0x00c5 ||
            code == 0x00e0 || code == 0x00e1 || code == 0x00e2 ||
            code == 0x00e3 || code == 0x00e4 || code == 0x00e5 ? 'A' :
            code == 0x00c7 || code == 0x00e7 ? 'C' :
            code == 0x00c8 || code == 0x00c9 || code == 0x00ca ||
            code == 0x00cb || code == 0x00e8 || code == 0x00e9 ||
            code == 0x00ea || code == 0x00eb ? 'E' :
            code == 0x00cc || code == 0x00cd || code == 0x00ce ||
            code == 0x00cf || code == 0x00ec || code == 0x00ed ||
            code == 0x00ee || code == 0x00ef ? 'I' :
            code == 0x00d1 || code == 0x00f1 ? 'N' :
            code == 0x00d2 || code == 0x00d3 || code == 0x00d4 ||
            code == 0x00d5 || code == 0x00d6 || code == 0x00f2 ||
            code == 0x00f3 || code == 0x00f4 || code == 0x00f5 ||
            code == 0x00f6 ? 'O' :
            code == 0x00d9 || code == 0x00da || code == 0x00db ||
            code == 0x00dc || code == 0x00f9 || code == 0x00fa ||
            code == 0x00fb || code == 0x00fc ? 'U' :
            code == 0x00dd || code == 0x00fd || code == 0x00ff ? 'Y' :
            '?';
        result.push_back(folded);
    }
    return result;
}

StringTable english_string_table() {
    return {"en", english_strings()};
}

LocalizationResult negotiate_localization(
    std::string_view requested_locale,
    const std::optional<std::filesystem::path>& language_file
) {
    const std::string requested = normalize_locale(requested_locale);
    if (language_file) {
        StringTable external = load_external_table(*language_file);
        if (locale_matches(requested, external.locale())) {
            return {
                std::move(external),
                requested,
                true,
            };
        }
    }
    return {
        english_string_table(),
        requested,
        false,
    };
}

}  // namespace aoe
