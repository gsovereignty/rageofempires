#!/usr/bin/env python3
"""Extract bounded UI font and icon contracts from the supplied HD decompile."""

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


FONT_CALL = re.compile(
    r"case (?P<slot>0x[0-9a-f]+|\d+):\s*"
    r"h = \(HGDIOBJ\)FUN_004f3010\("
    r"hdc,(?P<string>0x[0-9a-f]+|\d+),(?P<strike>[01]),"
    r'"(?P<name>RGE_FONT_[A-Z0-9_]+)"\);'
)
DIRECT_FONT = re.compile(
    r"case (?P<slot>0x[0-9a-f]+|\d+):\s*"
    r'h = \(HGDIOBJ\)FUN_004f11e0\(hdc,"(?P<family>[^"]+)",'
    r"(?P<height>0x[0-9a-f]+|\d+),(?P<weight>\d+),"
    r"(?P<italic>[01]),(?P<strike>[01])\);"
)
FONT_FILE = re.compile(r'AddFontResourceA\("(?P<path>data/fonts/[^"]+)"\);')
ICON_LOAD = re.compile(
    r'FUN_0050c6b0\("(?P<filename>[^"]+)",'
    r"(?P<resource>0x[0-9a-f]+|\d+),0\)"
)
ICON_STACK_LOAD = re.compile(
    r"ppuStack_78 = \(uint \*\*\)(?P<resource>0x[0-9a-f]+|\d+);\s*"
    r'pcStack_7c = "(?P<filename>[^"]+)";'
)


def integer(value):
    return int(value, 0)


def drs_slp_counts(path):
    data = Path(path).read_bytes()
    if len(data) < 64 or data[40:42] != b"1.":
        raise ValueError("invalid DRS")
    result = {}
    table_count = struct.unpack_from("<i", data, 56)[0]
    for table in range(table_count):
        offset = 64 + table * 12
        extension = data[offset:offset + 4][::-1].decode(
            "ascii", errors="strict"
        ).strip("\0 ").lower()
        info, count = struct.unpack_from("<ii", data, offset + 4)
        for entry in range(count):
            resource_id, payload, size = struct.unpack_from(
                "<iii", data, info + entry * 12
            )
            if payload < 0 or size < 0 or payload + size > len(data):
                raise ValueError("DRS entry outside archive")
            if extension == "slp":
                if size < 8:
                    raise ValueError("short SLP")
                result[resource_id] = struct.unpack_from("<I", data, payload + 4)[0]
    return result


def pe_string_table(path):
    """Read numeric RT_STRING values from a PE32 language DLL."""
    data = Path(path).read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    sections = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x10B:
        raise ValueError("language DLL is not PE32")
    section_table = optional + optional_size
    mappings = []
    for index in range(sections):
        at = section_table + index * 40
        virtual_size, virtual_address, raw_size, raw_offset = (
            struct.unpack_from("<IIII", data, at + 8)
        )
        mappings.append((
            virtual_address, max(virtual_size, raw_size), raw_offset
        ))

    def file_offset(rva):
        for virtual_address, size, raw_offset in mappings:
            if virtual_address <= rva < virtual_address + size:
                return raw_offset + rva - virtual_address
        raise ValueError("PE resource RVA outside sections")

    resource_rva = struct.unpack_from("<I", data, optional + 96 + 16)[0]
    resource_base = file_offset(resource_rva)

    def directory(relative):
        at = resource_base + relative
        named, identifiers = struct.unpack_from("<HH", data, at + 12)
        return [
            struct.unpack_from("<II", data, at + 16 + index * 8)
            for index in range(named + identifiers)
        ]

    result = {}
    string_type = next(
        (target for name, target in directory(0) if name == 6), None
    )
    if string_type is None:
        return result
    for block, block_target in directory(string_type & 0x7FFFFFFF):
        languages = directory(block_target & 0x7FFFFFFF)
        if not languages:
            continue
        _, language_target = languages[0]
        data_entry = resource_base + (language_target & 0x7FFFFFFF)
        payload_rva = struct.unpack_from("<I", data, data_entry)[0]
        position = file_offset(payload_rva)
        for index in range(16):
            length = struct.unpack_from("<H", data, position)[0]
            position += 2
            value = data[position:position + length * 2].decode("utf-16le")
            position += length * 2
            result[(block - 1) * 16 + index] = value
    return result


def extract(decompile_text, executable_path, interfac_path, language_dll=None):
    source = Path(executable_path).read_bytes()
    slps = drs_slp_counts(interfac_path)
    font_files = []
    for match in FONT_FILE.finditer(decompile_text):
        path = match.group("path")
        if path not in font_files:
            font_files.append(path)

    localized = pe_string_table(language_dll) if language_dll else {}
    slots = []
    for match in FONT_CALL.finditer(decompile_text):
        base = integer(match.group("string"))
        family = localized.get(base)
        height = localized.get(base + 1)
        style = localized.get(base + 2)
        resolved = None
        if family and height and style is not None:
            resolved = {
                "family": family,
                "height": int(height),
                "weight": 700 if "b" in style.lower() else 400,
                "italic": "i" in style.lower(),
            }
        slots.append({
            "slot": integer(match.group("slot")),
            "role": match.group("name"),
            "constructor": "localized_string_triplet",
            "string_ids": {
                "family": base,
                "height": base + 1,
                "style": base + 2,
            },
            "strikeout": match.group("strike") == "1",
            "resolved_values": resolved,
            "classification": (
                "exact_hd" if resolved else "exact_loader_contract"
            ),
        })
    for match in DIRECT_FONT.finditer(decompile_text):
        slots.append({
            "slot": integer(match.group("slot")),
            "role": None,
            "constructor": "direct",
            "family": match.group("family"),
            "height": integer(match.group("height")),
            "weight": integer(match.group("weight")),
            "italic": match.group("italic") == "1",
            "strikeout": match.group("strike") == "1",
            "classification": "exact_hd",
        })
    slots.sort(key=lambda item: item["slot"])

    wanted = {
        "btncmd.shp": "command_actions",
        "btntech.shp": "technologies",
        "ico_unit.shp": "units",
    }
    icon_sheets = {}
    for match in list(ICON_LOAD.finditer(decompile_text)) + list(
        ICON_STACK_LOAD.finditer(decompile_text)
    ):
        filename = match.group("filename")
        if filename not in wanted:
            continue
        resource_id = integer(match.group("resource"))
        key = (filename, resource_id)
        icon_sheets[key] = {
            "filename": filename,
            "resource_id": resource_id,
            "role": wanted[filename],
            "classification": "exact_executable_load",
            "present_in_interfac_drs": resource_id in slps,
            "frame_count": slps.get(resource_id),
            "dat_index_to_frame_contract": {
                "btncmd.shp": "partial_raw_action_constants",
                "btntech.shp": "identity_technology_record_plus_0x2c",
                "ico_unit.shp": "identity_ordinary_unit_record_plus_0x54",
            }[filename],
        }

    return {
        "schema": "aoe-ui-executable-evidence-v1",
        "source": {
            "executable": Path(executable_path).name,
            "executable_sha256": hashlib.sha256(source).hexdigest(),
            "decompile_function_font_setup": "FUN_004f3b30 at 0x004f3b30",
            "decompile_function_font_parser": "FUN_004f3010 at 0x004f3010",
            "language_dll": Path(language_dll).name if language_dll else None,
            "language_dll_sha256": (
                hashlib.sha256(Path(language_dll).read_bytes()).hexdigest()
                if language_dll else None
            ),
        },
        "fonts": {
            "slot_count": 37,
            "empty_slots": [5],
            "external_files": font_files,
            "slots": slots,
            "metrics": {
                "width": "GetTextMetricsA.tmAveCharWidth",
                "line_height": (
                    "GetTextMetricsA.tmHeight + "
                    "max(1, tmExternalLeading)"
                ),
                "classification": "exact_hd",
            },
            "localized_style_grammar": {
                "weight_700_tokens": ["B", "b"],
                "italic_tokens": ["I", "i"],
                "default_weight": 400,
                "classification": "exact_hd",
            },
            "renderer_api": {
                "creation": ["AddFontResourceA", "CreateFontIndirectA"],
                "measurement": ["GetTextMetricsA", "GetTextExtentPoint32A"],
                "drawing": ["TextOutA", "DrawTextA"],
                "colors": ["SetTextColor", "SetBkColor"],
                "classification": "exact_hd_imports_and_calls",
            },
        },
        "icon_sheets": sorted(
            icon_sheets.values(), key=lambda item: item["resource_id"]
        ),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("decompile")
    parser.add_argument("executable")
    parser.add_argument("interfac_drs")
    parser.add_argument("--language-dll")
    parser.add_argument(
        "--output", default="generated/ui_executable_evidence.json"
    )
    args = parser.parse_args()
    text = Path(args.decompile).read_text(errors="replace")
    output = extract(
        text, Path(args.executable), Path(args.interfac_drs),
        Path(args.language_dll) if args.language_dll else None
    )
    Path(args.output).write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()
