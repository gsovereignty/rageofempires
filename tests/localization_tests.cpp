#include "aoe/localization.hpp"

#include <filesystem>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <map>

namespace {

int failures{};

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename Callback>
void check_throws(Callback callback, std::string_view message) {
    try {
        callback();
        check(false, message);
    } catch (const std::exception&) {
    }
}

std::filesystem::path write_fixture(
    std::string_view name,
    std::string_view contents
) {
    const auto path =
        std::filesystem::temp_directory_path() / std::string{name};
    std::ofstream output(path, std::ios::trunc);
    output << contents;
    output.close();
    return path;
}

std::filesystem::path write_pe_fixture(
    std::string_view name,
    const std::vector<std::vector<std::uint16_t>>& strings
) {
    std::vector<std::uint8_t> bytes(0x500);
    const auto put16 = [&bytes](std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    };
    const auto put32 = [&bytes](std::size_t offset, std::uint32_t value) {
        for (int index = 0; index < 4; ++index) {
            bytes[offset + index] =
                static_cast<std::uint8_t>(value >> (index * 8));
        }
    };
    put16(0, 0x5a4d);
    put32(0x3c, 0x80);
    put32(0x80, 0x00004550);
    put16(0x86, 1);
    put16(0x94, 224);
    put16(0x98, 0x10b);
    put32(0x98 + 112, 0x1000);
    put32(0x98 + 116, 0x200);
    const std::size_t section = 0x98 + 224;
    put32(section + 8, 0x400);
    put32(section + 12, 0x1000);
    put32(section + 16, 0x400);
    put32(section + 20, 0x200);
    put16(0x200 + 14, 1);
    put32(0x200 + 16, 6);
    put32(0x200 + 20, 0x80000020);
    put16(0x220 + 14, 1);
    put32(0x220 + 16, 1);
    put32(0x220 + 20, 0x80000040);
    put16(0x240 + 14, 1);
    put32(0x240 + 16, 0x0409);
    put32(0x240 + 20, 0x60);
    put32(0x260, 0x1100);
    std::size_t cursor = 0x300;
    for (std::size_t slot = 0; slot < 16; ++slot) {
        const auto& value = slot < strings.size()
            ? strings[slot] : std::vector<std::uint16_t>{};
        put16(cursor, static_cast<std::uint16_t>(value.size()));
        cursor += 2;
        for (const std::uint16_t code : value) {
            put16(cursor, code);
            cursor += 2;
        }
    }
    put32(0x264, static_cast<std::uint32_t>(cursor - 0x300));
    const auto path =
        std::filesystem::temp_directory_path() / std::string{name};
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );
    return path;
}

}  // namespace

int main() {
    const aoe::StringTable english = aoe::english_string_table();
    check(english.locale() == "en", "English locale");
    check(english.text("hud.wood") == "WOOD", "English HUD string");
    check(
        english.format(
            "hud.population_paused",
            {{"population", "12"}, {"capacity", "20"}}
        ) == "POP 12/20 PAUSED",
        "dynamic HUD population template"
    );
    check(
        english.text("technology_tree.title") ==
            "CIVILIZATION TECHNOLOGY TREE",
        "technology browser chrome is localized"
    );
    check(
        english.text("editor.title") == "SCENARIO EDITOR",
        "scenario editor chrome is localized"
    );
    check(
        english.count_text(
            "browser.entry_one", "browser.entry_other", 1
        ) == "1 ENTRY",
        "singular grammar path"
    );
    check(
        english.count_text(
            "browser.entry_one", "browser.entry_other", 23
        ) == "23 ENTRIES",
        "plural grammar path"
    );
    check(
        english.count_text(
            "control_group.units_one", "control_group.units_other", 2,
            {{"group", "4"}}
        ) == "Group 4: 2 units",
        "plural grammar preserves named arguments"
    );
    check(english.contains("hud.wood"), "known key membership");
    check(!english.contains("missing"), "unknown key membership");
    check(
        aoe::shipped_language_profiles().size() == 11,
        "all shipped language profiles represented"
    );
    check(
        aoe::language_profile("pt-PT").audio_directory == "br",
        "regional locale selects packaged audio alias"
    );
    check(
        aoe::language_profile("ja-JP").windows_language_id == 0x0411,
        "locale selects exact Windows language ID"
    );
    check_throws(
        [] { (void)aoe::language_profile("ar"); },
        "unsupported shipped locale explicit"
    );
    check(
        aoe::plural_category("ru", 1) == aoe::PluralCategory::one &&
        aoe::plural_category("ru", 2) == aoe::PluralCategory::few &&
        aoe::plural_category("ru", 5) == aoe::PluralCategory::many &&
        aoe::plural_category("ru", 11) == aoe::PluralCategory::many &&
        aoe::plural_category("ru", 21) == aoe::PluralCategory::one,
        "Russian plural grammar"
    );
    check(
        aoe::plural_category("ja", 1) == aoe::PluralCategory::other &&
        aoe::plural_category("fr", 0) == aoe::PluralCategory::one,
        "East Asian and French plural grammar"
    );
    check(
        aoe::format_localized(
            "{player}: {count}", {{"player", "Ada"}, {"count", "3"}}
        ) == "Ada: 3",
        "named grammar arguments"
    );
    check(
        english.format(
            "technology_tree.title_civilization",
            {{"title", "TREE"}, {"civilization", "Britons"}}
        ) == "TREE: Britons",
        "cataloged dynamic title template"
    );
    check(
        aoe::stable_literal_key("OPTIONS") ==
            "ui.literal.87cc05b46a17cf65",
        "stable literal ID"
    );
    check_throws(
        [] { (void)aoe::format_localized("{missing}", {}); },
        "unresolved grammar argument rejected"
    );
    check(aoe::valid_utf8("ÁRVORE"), "valid UTF-8 accepted");
    check(
        !aoe::valid_utf8(std::string_view{"\xc0\xaf", 2}),
        "overlong UTF-8 rejected"
    );
    check(
        aoe::debug_font_fallback("Árvore 中") == "Arvore ?",
        "debug font folds Latin and marks missing glyph"
    );
    check(
        aoe::fit_localized_text("日本語の長い文字列", 5) == "日本語の…",
        "CJK layout truncates on code-point boundaries"
    );
    check(
        aoe::fit_localized_text("Árvore", 6) == "Árvore" &&
        aoe::fit_localized_text("Árvore", 4) == "Árv…" &&
        aoe::fit_localized_text("Árvore", 1) == "…",
        "Latin glyph layout and bounded ellipsis"
    );
    for (const std::size_t width : {38U, 48U, 66U, 68U, 72U, 78U,
                                   80U, 82U, 104U, 105U, 108U, 112U}) {
        const std::string fitted = aoe::fit_localized_text(
            "Очень длинная локализованная строка интерфейса", width
        );
        check(aoe::valid_utf8(fitted), "supported layout width stays UTF-8");
    }
    check_throws(
        [] {
            (void)aoe::debug_font_fallback(
                std::string_view{"\xed\xa0\x80", 3}
            );
        },
        "debug font rejects invalid UTF-8"
    );
    check_throws(
        [&] { (void)english.text("unknown"); },
        "unknown runtime key rejected"
    );

    const auto portuguese = write_fixture(
        "aoe-localization-pt.lang",
        "aoe-language 1\n"
        "locale \"pt-PT\"\n"
        "string \"hud.wood\" \"MADEIRA\"\n"
        "string \"hud.food\" \"PÃO\"\n"
        "string \"ui.objectives\" \"OBJETIVOS\"\n"
        "string \"objective.required\" \"OBRIGATORIO\"\n"
        "literal \"OPTIONS\" \"OPÇÕES\"\n"
    );
    const aoe::LocalizationResult selected =
        aoe::negotiate_localization("pt_PT", portuguese);
    check(selected.external_loaded, "external locale selected");
    check(selected.table.locale() == "pt-pt", "locale normalized");
    check(selected.table.text("hud.wood") == "MADEIRA", "override loaded");
    check(selected.table.text("hud.food") == "PÃO", "UTF-8 override loaded");
    check(
        selected.table.translate_literal("OPTIONS") == "OPÇÕES" &&
        selected.table.translate_literal("UNMAPPED") == "UNMAPPED",
        "every UI literal has stable override and English fallback"
    );
    check(
        selected.table.text("hud.gold") == "GOLD",
        "missing string falls back to English"
    );

    const aoe::LocalizationResult mismatch =
        aoe::negotiate_localization("de-DE", portuguese);
    check(!mismatch.external_loaded, "locale mismatch falls back");
    check(mismatch.table.locale() == "en", "mismatch uses English");
    check(mismatch.table.text("hud.wood") == "WOOD", "English deterministic");

    const auto duplicate = write_fixture(
        "aoe-localization-duplicate.lang",
        "aoe-language 1\nlocale \"pt\"\n"
        "string \"hud.wood\" \"A\"\n"
        "string \"hud.wood\" \"B\"\n"
    );
    check_throws(
        [&] { (void)aoe::negotiate_localization("pt", duplicate); },
        "duplicate key rejected"
    );
    const auto unknown = write_fixture(
        "aoe-localization-unknown.lang",
        "aoe-language 1\nlocale \"pt\"\n"
        "string \"private.key\" \"NO\"\n"
    );
    check_throws(
        [&] { (void)aoe::negotiate_localization("pt", unknown); },
        "unknown key rejected"
    );
    const auto control = write_fixture(
        "aoe-localization-control.lang",
        "aoe-language 1\nlocale \"pt\"\n"
        "string \"hud.wood\" \"bad\tvalue\"\n"
    );
    check_throws(
        [&] { (void)aoe::negotiate_localization("pt", control); },
        "control byte rejected"
    );
    check_throws(
        [] { (void)aoe::negotiate_localization("../pt"); },
        "invalid locale rejected"
    );

    const auto base_dll = write_pe_fixture(
        "aoe-language-base.dll",
        {{'B', 'A', 'S', 'E'}, {0x00c1}}
    );
    const auto expansion_dll = write_pe_fixture(
        "aoe-language-x1.dll",
        {{'E', 'X', 'P'}}
    );
    const auto exact = aoe::extract_pe_string_resources(base_dll, 0x0409);
    check(exact.at(0) == "BASE", "exact RT_STRING ID decoded");
    check(exact.at(1) == "\xc3\x81", "UTF-16 converted to UTF-8");
    const aoe::LegacyLanguageReport legacy =
        aoe::load_legacy_language_sources(
            "en-US", 0x0409, {base_dll, expansion_dll},
            {{0, "hud.wood"}}
        );
    check(legacy.table.text("hud.wood") == "EXP", "later DLL precedence");
    check(legacy.unknown.at(1) == "\xc3\x81", "unknown ID reported");
    check(legacy.sources.size() == 2, "external sources reported");
    check(
        aoe::legacy_ui_string_catalog().size() == 7,
        "proven menu ID catalog complete"
    );

    const std::map<std::uint32_t, std::string> font_strings{
        {119, "Georgia"}, {120, "9"}, {121, "BI"},
        {200, "Georgia"}, {201, "9"}, {202, "B"},
    };
    const auto fonts = aoe::extract_legacy_font_styles(font_strings);
    check(
        fonts.at(7).family == "Georgia" &&
        fonts.at(7).pixel_height == 9 && fonts.at(7).bold &&
        fonts.at(7).italic,
        "localized font triplet grammar"
    );
    check(
        fonts.at(32).bold && !fonts.at(32).italic,
        "technology-tree font slot mapping"
    );
    check_throws(
        [] {
            (void)aoe::extract_legacy_font_styles({
                {119, "Georgia"}, {120, "large"}, {121, "B"}
            });
        },
        "invalid localized font height rejected"
    );

    const auto archive_root =
        std::filesystem::temp_directory_path() / "aoe-locale-assets";
    std::filesystem::create_directories(archive_root / "Bin" / "br");
    {
        std::ofstream output(archive_root / "Bin" / "br" / "language.dll");
        output << "fixture";
    }
    const auto discovered = aoe::discover_legacy_language_sources(
        archive_root, "pt-BR"
    );
    check(
        discovered.size() == 1 &&
        discovered.front().parent_path().filename() == "br",
        "locale-specific packaged archive discovery"
    );
    check(
        aoe::discover_legacy_language_sources(archive_root, "de").empty(),
        "missing locale archive has explicit empty result"
    );
    const auto scenario_audio = aoe::localized_audio_directories(
        archive_root, "pt-BR", "scenario"
    );
    check(
        scenario_audio.size() == 2 &&
        scenario_audio[0] == archive_root / "Sound" / "scenario" / "br" &&
        scenario_audio[1] == archive_root / "Sound" / "scenario" / "en",
        "localized narration precedes packaged English fallback"
    );
    const auto english_taunts = aoe::localized_audio_directories(
        archive_root, "en-US", "taunt"
    );
    check(
        english_taunts.size() == 1 &&
        english_taunts[0] == archive_root / "Taunt" / "en",
        "English audio fallback has no duplicate probe"
    );
    check_throws(
        [&] {
            (void)aoe::localized_audio_directories(
                archive_root, "en", "unknown"
            );
        },
        "unknown localized audio family rejected"
    );
    check_throws(
        [&] {
            (void)aoe::load_legacy_language_sources(
                "en", 0x0409, {base_dll}, {{0, "private.key"}}
            );
        },
        "unknown mapping key rejected"
    );
    const auto malformed_dll = write_pe_fixture(
        "aoe-language-malformed.dll", {{0xd800}}
    );
    check_throws(
        [&] {
            (void)aoe::extract_pe_string_resources(
                malformed_dll, 0x0409
            );
        },
        "invalid UTF-16 rejected"
    );

    std::filesystem::remove(portuguese);
    std::filesystem::remove(duplicate);
    std::filesystem::remove(unknown);
    std::filesystem::remove(control);
    std::filesystem::remove(base_dll);
    std::filesystem::remove(expansion_dll);
    std::filesystem::remove(malformed_dll);
    std::filesystem::remove_all(archive_root);
    return failures == 0 ? 0 : 1;
}
