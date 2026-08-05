#!/usr/bin/env python3
"""Generate stable IDs for reconstruction-owned C++ UI literals."""

import argparse
import ast
import json
import pathlib
import re


STRING = re.compile(r'"(?:\\.|[^"\\])*"')


def stable_key(text: str) -> str:
    value = 14695981039346656037
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"ui.literal.{value:016x}"


def literals(source: pathlib.Path) -> set[str]:
    result = set()
    for token in STRING.findall(source.read_text(encoding="utf-8")):
        try:
            value = ast.literal_eval(token)
        except (SyntaxError, ValueError):
            continue
        if value and "\n" not in value and "\r" not in value:
            result.add(value)
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path, nargs="+")
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    by_key: dict[str, str] = {}
    for source in args.source:
        for text in literals(source):
            key = stable_key(text)
            if key in by_key and by_key[key] != text:
                raise SystemExit(f"stable localization ID collision: {key}")
            by_key[key] = text
    body = "# key\tUTF-8 source literal (JSON)\n" + "".join(
        f"{key}\t{json.dumps(text, ensure_ascii=False)}\n"
        for key, text in sorted(by_key.items())
    )
    args.output.write_text(body, encoding="utf-8")


if __name__ == "__main__":
    main()
