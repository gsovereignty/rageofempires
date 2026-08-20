#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
archive="$repo_root/docs/evidence/decompiled-source/decompiled.tar.xz"
checksum="$repo_root/docs/evidence/decompiled-source/decompiled.tar.xz.sha256"
destination="$repo_root/decompiled"
staging=$(mktemp -d "$repo_root/.decompiled-extract.XXXXXX")

cleanup() {
    rm -rf -- "$staging"
}
trap cleanup EXIT HUP INT TERM

if command -v sha256sum >/dev/null 2>&1; then
    (cd "$(dirname -- "$archive")" && sha256sum -c "$(basename -- "$checksum")")
else
    (cd "$(dirname -- "$archive")" && shasum -a 256 -c "$(basename -- "$checksum")")
fi
tar -xJf "$archive" -C "$staging"

if [ ! -d "$staging/decompiled" ]; then
    echo "archive does not contain decompiled/" >&2
    exit 1
fi

rm -rf -- "$destination"
mv -- "$staging/decompiled" "$destination"

echo "$destination"
