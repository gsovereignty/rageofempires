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
        std::map<std::string, std::string> strings,
        std::map<std::uint32_t, std::string> numeric_strings = {}
    );

    [[nodiscard]] const std::string& locale() const { return locale_; }
    [[nodiscard]] std::string_view text(std::string_view key) const;
    [[nodiscard]] bool contains(std::string_view key) const noexcept;
    [[nodiscard]] bool has_literal_overrides() const noexcept {
        return has_literal_overrides_;
    }
    [[nodiscard]] std::string translate_literal(std::string_view english) const;
    [[nodiscard]] std::string text_or(
        std::uint32_t numeric_id,
        std::string_view fallback
    ) const;
    [[nodiscard]] bool contains(std::uint32_t numeric_id) const noexcept;
    [[nodiscard]] const std::map<std::string, std::string>& strings() const {
        return strings_;
    }
    [[nodiscard]] std::string count_text(
        std::string_view singular_key,
        std::string_view plural_key,
        std::int64_t count
    ) const;
    [[nodiscard]] std::string format(
        std::string_view key,
        const std::map<std::string, std::string>& arguments
    ) const;

private:
    std::string locale_;
    std::map<std::string, std::string> strings_;
    bool has_literal_overrides_{};
    std::map<std::uint32_t, std::string> numeric_strings_;
};

enum class PluralCategory { zero, one, two, few, many, other };

struct LanguageProfile {
    std::string locale;
    std::uint16_t windows_language_id{};
    std::vector<std::string> archive_directories;
    std::string audio_directory;
    bool right_to_left{};
};

struct LegacyFontStyle {
    std::string family;
    int pixel_height{};
    bool bold{};
    bool italic{};
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
    std::map<std::uint32_t, LegacyFontStyle> fonts;
};

[[nodiscard]] const std::vector<LanguageProfile>& shipped_language_profiles();
[[nodiscard]] const LanguageProfile& language_profile(std::string_view locale);
[[nodiscard]] PluralCategory plural_category(
    std::string_view locale,
    std::int64_t count
);
[[nodiscard]] std::string format_localized(
    std::string_view pattern,
    const std::map<std::string, std::string>& arguments
);
[[nodiscard]] std::string stable_literal_key(std::string_view english);
[[nodiscard]] std::string fit_localized_text(
    std::string_view text,
    std::size_t maximum_codepoints,
    bool ellipsis = true
);
[[nodiscard]] std::map<std::uint32_t, std::string> legacy_ui_string_catalog();
[[nodiscard]] std::vector<std::filesystem::path> discover_legacy_language_sources(
    const std::filesystem::path& packaged_asset_root,
    std::string_view locale
);
[[nodiscard]] std::vector<std::filesystem::path> localized_audio_directories(
    const std::filesystem::path& packaged_asset_root,
    std::string_view locale,
    std::string_view category
);
[[nodiscard]] std::map<std::uint32_t, LegacyFontStyle> extract_legacy_font_styles(
    const std::map<std::uint32_t, std::string>& strings
);

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
