#pragma once

#include <filesystem>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aoe {

class StringTable {
public:
    StringTable(
        std::string locale,
        std::map<std::string, std::string> strings
    );

    [[nodiscard]] const std::string& locale() const { return locale_; }
    [[nodiscard]] std::string_view text(std::string_view key) const;
    [[nodiscard]] std::string count_text(
        std::string_view singular_key,
        std::string_view plural_key,
        std::int64_t count
    ) const;

private:
    std::string locale_;
    std::map<std::string, std::string> strings_;
};

[[nodiscard]] bool valid_utf8(std::string_view text) noexcept;
// Current SDL debug-font renderer is ASCII-only. Preserve ASCII, fold common
// Latin glyphs, and mark every other code point explicitly instead of
// emitting malformed bytes or silently dropping text.
[[nodiscard]] std::string debug_font_fallback(std::string_view text);

struct LocalizationResult {
    StringTable table;
    std::string requested_locale;
    bool external_loaded{};
};

struct LegacyLanguageReport {
    StringTable table;
    std::map<std::uint32_t, std::string> extracted;
    std::map<std::uint32_t, std::string> unknown;
    std::vector<std::filesystem::path> sources;
    std::uint16_t language_id{};
};

std::map<std::uint32_t, std::string> extract_pe_string_resources(
    const std::filesystem::path& external_dll,
    std::uint16_t language_id
);
LegacyLanguageReport load_legacy_language_sources(
    std::string_view locale,
    std::uint16_t language_id,
    const std::vector<std::filesystem::path>& external_dlls,
    const std::map<std::uint32_t, std::string>& id_to_key
);

StringTable english_string_table();
LocalizationResult negotiate_localization(
    std::string_view requested_locale,
    const std::optional<std::filesystem::path>& language_file = std::nullopt
);

}  // namespace aoe
