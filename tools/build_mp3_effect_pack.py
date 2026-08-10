#!/usr/bin/env python3
"""Convert resolved legacy DRS WAV effects into shared runtime MP3 assets."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from build_web_asset_pack import read_drs


def convert(job: tuple[int, bytes], output: Path, ffmpeg: str) -> dict[str, object]:
    resource_id, wav = job
    destination = output / f"{resource_id}.mp3"
    with tempfile.NamedTemporaryFile(suffix=".wav") as source:
        source.write(wav)
        source.flush()
        subprocess.run(
            [
                ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                "-i", source.name, "-map_metadata", "-1", "-codec:a",
                "libmp3lame", "-q:a", "4", "-write_xing", "0",
                str(destination),
            ],
            check=True,
        )
    payload = destination.read_bytes()
    return {
        "resource_id": resource_id,
        "byte_size": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
    }


def build(root: Path, output: Path, ffmpeg: str, workers: int) -> dict[str, object]:
    catalog = json.loads((root / "generated/audio_catalog.json").read_text())
    wanted: dict[int, str] = {}
    for sound in catalog["sounds"]:
        for item in sound["items"]:
            if item["available"]:
                wanted[int(item["resource_id"])] = item["resolved_archive"]

    archives: dict[str, dict[tuple[str, int], bytes]] = {}
    for archive in sorted(set(wanted.values())):
        _, resources = read_drs(root / "game_data/Data" / archive)
        archives[archive] = resources
    jobs = [
        (resource_id, archives[archive][("wav", resource_id)])
        for resource_id, archive in sorted(wanted.items())
    ]

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)
    with ThreadPoolExecutor(max_workers=workers) as executor:
        records = list(executor.map(
            lambda job: convert(job, output, ffmpeg), jobs
        ))
    manifest = {
        "schema": "aoe-mp3-effects-v1",
        "encoder": "libmp3lame-q4-no-xing",
        "effect_count": len(records),
        "total_bytes": sum(int(record["byte_size"]) for record in records),
        "effects": records,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=Path.cwd())
    parser.add_argument(
        "--output", type=Path,
        default=Path("game_data/Sound/effects"),
    )
    parser.add_argument("--ffmpeg", default="ffmpeg")
    parser.add_argument("--workers", type=int, default=8)
    arguments = parser.parse_args()
    build(
        arguments.source_root.resolve(), arguments.output.resolve(),
        arguments.ffmpeg, arguments.workers,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
