#include "aoe/localization.hpp"

#include <algorithm>
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
    return {
        StringTable(normalized, std::move(translated)),
        std::move(extracted),
        std::move(unknown),
        external_dlls,
        language_id,
    };
}

StringTable::StringTable(
    std::string locale,
    std::map<std::string, std::string> strings
) : locale_(std::move(locale)), strings_(std::move(strings)) {
    if (locale_.empty() || strings_.empty()) {
        throw std::invalid_argument("empty string table");
    }
}

std::string_view StringTable::text(std::string_view key) const {
    const auto found = strings_.find(std::string{key});
    if (found == strings_.end()) {
        throw std::out_of_range("unknown localization key");
    }
    return found->second;
}

std::string StringTable::count_text(
    std::string_view singular_key,
    std::string_view plural_key,
    std::int64_t count
) const {
    std::string result{
        text(count == 1 ? singular_key : plural_key)
    };
    constexpr std::string_view marker{"{count}"};
    std::size_t position{};
    const std::string replacement = std::to_string(count);
    while ((position = result.find(marker, position)) !=
           std::string::npos) {
        result.replace(position, marker.size(), replacement);
        position += replacement.size();
    }
    return result;
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
