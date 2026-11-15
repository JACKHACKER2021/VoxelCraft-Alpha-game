cmake_minimum_required(VERSION 3.22.1)
project(voxelcraft CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# 2.2.0: eSpeak NG (GPL-3.0-or-later), the voice of the Rozhlas. Only the library; no sound output of its own.
set(ESPEAK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/espeak-ng)
file(GLOB UCD_SOURCES ${ESPEAK_DIR}/ucd-tools/src/*.c)
add_library(espeak STATIC
    ${UCD_SOURCES}
    ${ESPEAK_DIR}/libespeak-ng/common.c
    ${ESPEAK_DIR}/libespeak-ng/mnemonics.c
    ${ESPEAK_DIR}/libespeak-ng/error.c
    ${ESPEAK_DIR}/libespeak-ng/ieee80.c
    ${ESPEAK_DIR}/libespeak-ng/compiledata.c
    ${ESPEAK_DIR}/libespeak-ng/compiledict.c
    ${ESPEAK_DIR}/libespeak-ng/dictionary.c
    ${ESPEAK_DIR}/libespeak-ng/encoding.c
    ${ESPEAK_DIR}/libespeak-ng/intonation.c
    ${ESPEAK_DIR}/libespeak-ng/langopts.c
    ${ESPEAK_DIR}/libespeak-ng/numbers.c
    ${ESPEAK_DIR}/libespeak-ng/phoneme.c
    ${ESPEAK_DIR}/libespeak-ng/phonemelist.c
    ${ESPEAK_DIR}/libespeak-ng/readclause.c
    ${ESPEAK_DIR}/libespeak-ng/setlengths.c
    ${ESPEAK_DIR}/libespeak-ng/soundicon.c
    ${ESPEAK_DIR}/libespeak-ng/spect.c
    ${ESPEAK_DIR}/libespeak-ng/ssml.c
    ${ESPEAK_DIR}/libespeak-ng/synthdata.c
    ${ESPEAK_DIR}/libespeak-ng/synthesize.c
    ${ESPEAK_DIR}/libespeak-ng/tr_languages.c
    ${ESPEAK_DIR}/libespeak-ng/translate.c
    ${ESPEAK_DIR}/libespeak-ng/translateword.c
    ${ESPEAK_DIR}/libespeak-ng/voices.c
    ${ESPEAK_DIR}/libespeak-ng/wavegen.c
    ${ESPEAK_DIR}/libespeak-ng/speech.c
    ${ESPEAK_DIR}/libespeak-ng/espeak_api.c
    ${ESPEAK_DIR}/libespeak-ng/klatt.c
)
target_include_directories(espeak PUBLIC ${ESPEAK_DIR}/include PRIVATE ${ESPEAK_DIR}/include/compat ${ESPEAK_DIR}/config ${ESPEAK_DIR}/ucd-tools/src/include)
target_compile_definitions(espeak PUBLIC LIBESPEAK_NG_EXPORT=1 PRIVATE "PATH_ESPEAK_DATA=\"/nonexistent\"")
target_compile_options(espeak PRIVATE -fwrapv -w)

add_library(voxelcraft SHARED
    main.cpp
    engine/Noise.cpp
    engine/World.cpp
    engine/Mesher.cpp
    engine/Player.cpp
    engine/Portal.cpp
    engine/TexturePack.cpp
    engine/FallbackAtlas.cpp
    engine/Renderer.cpp
    engine/Game.cpp
    engine/Session.cpp
    engine/Items.cpp
    engine/Save.cpp
    engine/Font.cpp
    engine/Ui.cpp
    engine/Crafting.cpp
    engine/Json.cpp
    engine/GenConfig.cpp
    engine/Packs.cpp
    engine/Shaders.cpp
    engine/Skin.cpp
    engine/Profile.cpp
    engine/Store.cpp
    engine/Audio.cpp
    engine/Voice.cpp
    engine/PlayerModel.cpp
    engine/Png.cpp
    engine/Particles.cpp
    engine/Nbt.cpp
    engine/Structure.cpp
    engine/McWorld.cpp
    engine/Drops.cpp
    engine/FirstPerson.cpp
    engine/Lang.cpp
    engine/ItemModel.cpp
    engine/Developer.cpp
    engine/Minimap.cpp
    engine/BlockShapes.cpp
    engine/Loot.cpp
    engine/WorldStructures.cpp
    engine/Boats.cpp
    engine/Trains.cpp
    engine/Achievements.cpp
    engine/Net.cpp
    engine/Multiplayer.cpp
    engine/Commands.cpp
    engine/Sky.cpp
    engine/Mobs.cpp
    engine/Trees.cpp
    engine/Rewards.cpp
    engine/Trading.cpp
    engine/Mods.cpp
    engine/JavaImport.cpp
    engine/WorldFiles.cpp)

target_include_directories(voxelcraft PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ANDROID_NDK}/sources/android/native_app_glue)
target_compile_options(voxelcraft PRIVATE -Wall -Wextra -O2)
# 2.1.0: the preview edition (gradle flavor "preview") turns on the next version's features.
option(VC_CLASSIC "Build VoxelCraft Classic 0.0.3" OFF)
if(VC_CLASSIC)
    target_compile_definitions(voxelcraft PRIVATE VC_CLASSIC=1)
endif()
option(VC_PREVIEW "Build the VoxelCraft Preview edition" OFF)
if(VC_PREVIEW)
    target_compile_definitions(voxelcraft PRIVATE VC_PREVIEW=1)
endif()

# Keep ANativeActivity_onCreate from being stripped, and use 16 KB page
# alignment which Android 15+ devices require.
target_link_options(voxelcraft PRIVATE
    -u ANativeActivity_onCreate
    -Wl,-z,max-page-size=16384)

target_compile_definitions(voxelcraft PRIVATE VC_ESPEAK=1)
target_link_libraries(voxelcraft espeak native_app_glue android EGL GLESv3 log z)
