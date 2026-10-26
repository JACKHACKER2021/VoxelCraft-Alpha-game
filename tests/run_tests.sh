#!/usr/bin/env bash
# Compiles the engine for the host (with a no-op renderer) and runs the tests.
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=app/src/main/cpp
NDK_INC=$(ls -d "${ANDROID_HOME:-/opt/android-sdk}"/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include | tail -1)
mkdir -p build/tests/include
# Only the GLES/KHR headers are borrowed from the NDK, for the type declarations.
ln -sfn "$NDK_INC/GLES3" build/tests/include/GLES3
ln -sfn "$NDK_INC/KHR" build/tests/include/KHR
ENGINE=("$SRC"/engine/{Noise,World,Mesher,Player,Portal,TexturePack,FallbackAtlas,Items,Save,Font,Ui,Session,Game,Crafting,Json,GenConfig,Packs,Skin,Profile,Store,Audio,PlayerModel,Png,Particles,Nbt,Structure,Drops,FirstPerson,Lang,ItemModel,Developer,Minimap,BlockShapes,Loot,WorldStructures,Boats,Achievements,Net,Multiplayer,Commands,Sky,Mobs,Trees,Rewards,Trading,Mods,JavaImport,WorldFiles,Shaders}.cpp)
CXX="g++ -std=c++17 -O1 -g -Wall -Wextra -I$SRC -isystem build/tests/include"
$CXX tests/core_test.cpp tests/renderer_stub.cpp tests/png_writer.cpp "${ENGINE[@]}" -o build/tests/core_test
./build/tests/core_test
# Optional: PNG snapshots of every menu screen (build/snapshots).
if [ "${SNAPSHOTS:-0}" = 1 ]; then
    $CXX tests/ui_snapshots.cpp tests/renderer_stub.cpp tests/png_writer.cpp "${ENGINE[@]}" -o build/tests/ui_snapshots
    ./build/tests/ui_snapshots build/snapshots
    $CXX -O2 tests/world_snapshots.cpp tests/png_writer.cpp "${ENGINE[@]}" tests/renderer_stub.cpp -o build/tests/world_snapshots
    ./build/tests/world_snapshots build/snapshots
fi
