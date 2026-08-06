#!/usr/bin/env bash
set -euo pipefail

readonly EMSDK_VERSION="4.0.10"
readonly EMSDK_COMMIT="62a853cd3b3134398ce85cde8bb5cbb2ef0194cb"
readonly REPOSITORY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly EMSDK_ROOT="${REPOSITORY_ROOT}/build-web/emsdk"

mkdir -p "${REPOSITORY_ROOT}/build-web"

if [[ ! -d "${EMSDK_ROOT}/.git" ]]; then
    git clone \
        --branch "${EMSDK_VERSION}" \
        --depth 1 \
        https://github.com/emscripten-core/emsdk.git \
        "${EMSDK_ROOT}"
fi

actual_commit="$(git -C "${EMSDK_ROOT}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${EMSDK_COMMIT}" ]]; then
    echo "Unexpected emsdk revision: ${actual_commit}" >&2
    exit 1
fi

"${EMSDK_ROOT}/emsdk" install "${EMSDK_VERSION}"
"${EMSDK_ROOT}/emsdk" activate "${EMSDK_VERSION}"
echo "Run: source '${EMSDK_ROOT}/emsdk_env.sh'"
