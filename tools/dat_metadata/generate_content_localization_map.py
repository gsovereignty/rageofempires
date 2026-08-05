#!/usr/bin/env python3
"""Emit exhaustive DAT content-to-language-DLL consumer bindings."""

import argparse
import json
from pathlib import Path


def numeric(value):
    if value is None:
        return 0
    if isinstance(value, int):
        return value
    digits = "".join(character for character in value if character.isdigit())
    return int(digits) if digits else 0


def generate(catalog):
    rows = {("object", item["id"], numeric(item["language_dll_name"]),
            numeric(item["language_dll_help"]), 0)
            for item in catalog["object_variants"]}
    rows.update(("technology", item["id"],
                 numeric(item["language_name_id"]),
                 numeric(item["language_help_id"]),
                 numeric(item["language_description_id"]))
                for item in catalog["technologies"])
    return sorted(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    rows = generate(json.loads(args.catalog.read_text(encoding="utf-8")))
    body = "kind\tDAT ID\tname ID\thelp ID\tdescription ID\n"
    body += "".join("\t".join(map(str, row)) + "\n" for row in rows)
    args.output.write_text(body, encoding="utf-8")


if __name__ == "__main__":
    main()
