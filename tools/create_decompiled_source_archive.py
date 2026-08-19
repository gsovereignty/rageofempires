#!/usr/bin/env python3
"""Create byte-reproducible decompiled-source archive and checksum."""

from __future__ import annotations

import hashlib
import lzma
from pathlib import Path
import sys
import tarfile
import tempfile


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_DIR = ROOT / "resources" / "decompiled-source"
ARCHIVE = OUTPUT_DIR / "decompiled.tar.xz"
CHECKSUM = OUTPUT_DIR / "decompiled.tar.xz.sha256"


def normalized_info(path: Path, arcname: str) -> tarfile.TarInfo:
    info = tarfile.TarInfo(arcname)
    info.uid = 0
    info.gid = 0
    info.uname = "root"
    info.gname = "root"
    info.mtime = 0
    info.mode = path.stat().st_mode & 0o777
    if path.is_dir():
        info.type = tarfile.DIRTYPE
    else:
        info.type = tarfile.REGTYPE
        info.size = path.stat().st_size
    return info


def source_entries(source: Path) -> list[Path]:
    return [
        source,
        *sorted(
            (path for path in source.rglob("*") if path.name != ".DS_Store"),
            key=lambda path: path.relative_to(source).as_posix(),
        ),
    ]


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} SOURCE_DIRECTORY", file=sys.stderr)
        return 2

    source = Path(sys.argv[1]).resolve()
    if not source.is_dir():
        print(f"not a directory: {source}", file=sys.stderr)
        return 2

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=OUTPUT_DIR, delete=False) as temporary:
        temporary_path = Path(temporary.name)

    try:
        with lzma.open(temporary_path, "wb", preset=9) as compressed:
            with tarfile.open(fileobj=compressed, mode="w|") as archive:
                for path in source_entries(source):
                    relative = path.relative_to(source)
                    arcname = "decompiled" if relative == Path(".") else (
                        Path("decompiled") / relative
                    ).as_posix()
                    info = normalized_info(path, arcname)
                    if path.is_dir():
                        archive.addfile(info)
                    else:
                        with path.open("rb") as source_file:
                            archive.addfile(info, source_file)
        temporary_path.replace(ARCHIVE)
    finally:
        temporary_path.unlink(missing_ok=True)

    digest = hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()
    CHECKSUM.write_text(f"{digest}  {ARCHIVE.name}\n", encoding="ascii")
    print(f"{ARCHIVE} {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
