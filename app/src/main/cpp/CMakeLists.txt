cmake_minimum_required(VERSION 3.22.1)
project(voxelcraft CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

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
    engine/PlayerModel.cpp
    engine/Png.cpp
    engine/Particles.cpp
    engine/Nbt.cpp
    engine/Structure.cpp
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

# Keep ANativeActivity_onCreate from being stripped, and use 16 KB page
# alignment which Android 15+ devices require.
target_link_options(voxelcraft PRIVATE
    -u ANativeActivity_onCreate
    -Wl,-z,max-page-size=16384)

target_link_libraries(voxelcraft native_app_glue android EGL GLESv3 log z)
