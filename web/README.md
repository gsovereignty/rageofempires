# Browser risk spike

This directory contains the static browser shell for the fixed risk-spike
scenario. Browser support is disabled unless both Emscripten and
`AOE_BUILD_WEB=ON` are selected.

Pinned dependencies:

- Emscripten SDK: `4.0.10` (`web/emscripten-version.txt`)
- SDL3: `SDL-3.4.12-release-3.4.12`, from the tracked
  `third_party/SDL-3.4.12-f87239e.tar.gz` archive with its SHA-256 checked by
  CMake

Bootstrap installs only the recorded tag and verifies emsdk commit
`62a853cd3b3134398ce85cde8bb5cbb2ef0194cb`:

```sh
./web/bootstrap_emsdk.sh
source build-web/emsdk/emsdk_env.sh
```

Then configure only in `build-web/`:

```sh
emcmake cmake -S . -B build-web \
  -DCMAKE_BUILD_TYPE=Release \
  -DAOE_BUILD_WEB=ON \
  -DAOE_ENABLE_MPG123=OFF
cmake --build build-web --target aoe_web
cmake --build build-web --target web_risk_spike
```

Generated browser files belong only in `build-web/dist/`; they are not native
runtime or package inputs. Serve that directory through static HTTP rather
than opening its HTML through `file://`.
