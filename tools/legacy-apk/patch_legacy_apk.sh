#!/usr/bin/env bash
# Prepares YOUR OWN copy of an old Pocket Edition APK (e.g. 1.1.2) for
# installation on a modern Android phone (Android 14-16).
#
# The result is for personal use only: it still contains the original
# game code, so never share or upload the patched APK.
#
# Usage:
#   ./patch_legacy_apk.sh check   <original.apk>   # what is inside, will it run?
#   ./patch_legacy_apk.sh install <original.apk>   # install unmodified (fastest)
#   ./patch_legacy_apk.sh patch   <original.apk> [--rename]
#                                                  # raise targetSdk, re-sign
#
# Requirements: java, adb, and Android build-tools (zipalign, apksigner,
# aapt2) on PATH or in $ANDROID_HOME. apktool is downloaded automatically.
set -euo pipefail

APKTOOL_VERSION=2.11.1
NEW_TARGET_SDK=28        # >= 24 passes the Android 15+ install block, <= 28 keeps old storage behaviour
LEGACY_PACKAGE=com.mojang.minecraftpe.legacy

here="$(cd "$(dirname "$0")" && pwd)"
work="$here/work"

die() { echo "ERROR: $*" >&2; exit 1; }
info() { echo "==> $*"; }

find_build_tool() {
    local name=$1
    if command -v "$name" >/dev/null 2>&1; then command -v "$name"; return; fi
    local sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    [ -n "$sdk" ] || return 1
    ls -d "$sdk"/build-tools/*/"$name" 2>/dev/null | sort -V | tail -1
}

apktool() {
    local jar="${APKTOOL_JAR:-$here/apktool_$APKTOOL_VERSION.jar}"
    if [ ! -f "$jar" ]; then
        info "Downloading apktool $APKTOOL_VERSION"
        curl -fsSL -o "$jar" \
            "https://github.com/iBotPeaches/Apktool/releases/download/v$APKTOOL_VERSION/apktool_$APKTOOL_VERSION.jar"
    fi
    java -jar "$jar" "$@"
}

device_info() {
    command -v adb >/dev/null 2>&1 || { echo "  (adb not found, skipping device check)"; return; }
    if ! adb get-state >/dev/null 2>&1; then
        echo "  (no phone connected over USB debugging, skipping device check)"
        return
    fi
    local sdk abi32
    sdk=$(adb shell getprop ro.build.version.sdk | tr -d '\r')
    abi32=$(adb shell getprop ro.product.cpu.abilist32 | tr -d '\r')
    echo "  Phone: Android API $sdk"
    if [ -n "$abi32" ]; then
        echo "  32-bit apps: SUPPORTED ($abi32)"
    else
        echo "  32-bit apps: NOT SUPPORTED - a 32-bit-only APK cannot run on this phone,"
        echo "  no matter how it is patched. Use VoxelCraft (path C) instead."
    fi
}

cmd_check() {
    local apk=$1
    info "APK: $apk"
    local abis
    abis=$(unzip -Z1 "$apk" 'lib/*' 2>/dev/null | cut -d/ -f2 | sort -u | tr '\n' ' ')
    echo "  Native code for: ${abis:-none}"
    case " $abis " in
        *" arm64-v8a "*) echo "  64-bit ARM included: runs on 64-bit-only phones too." ;;
        *) echo "  No arm64-v8a: needs a phone that still supports 32-bit apps." ;;
    esac
    local aapt2
    if aapt2=$(find_build_tool aapt2) && [ -n "$aapt2" ]; then
        "$aapt2" dump badging "$apk" 2>/dev/null | grep -E "^package:|^sdkVersion|^targetSdkVersion" | sed 's/^/  /'
    fi
    device_info
}

cmd_install() {
    local apk=$1
    info "Installing unmodified APK (Android 14+ needs the low-targetSdk bypass flag)"
    adb install --bypass-low-target-sdk-block "$apk" ||
        die "Install failed. Try: $0 patch \"$apk\""
}

cmd_patch() {
    local apk=$1 rename=${2:-}
    local zipalign apksigner
    zipalign=$(find_build_tool zipalign) || die "zipalign not found (install Android build-tools)"
    apksigner=$(find_build_tool apksigner) || die "apksigner not found (install Android build-tools)"

    rm -rf "$work"
    mkdir -p "$work"
    info "Decoding"
    # Resources must be decoded too, otherwise the manifest stays binary.
    apktool d -f -o "$work/decoded" "$apk"

    local yml="$work/decoded/apktool.yml"
    info "Setting targetSdkVersion to $NEW_TARGET_SDK"
    if grep -qE "^ +targetSdkVersion:" "$yml"; then
        sed -i -E "s/^( +targetSdkVersion:).*/\1 '$NEW_TARGET_SDK'/" "$yml"
    else
        sed -i -E "s/^sdkInfo:.*/sdkInfo:\n  targetSdkVersion: '$NEW_TARGET_SDK'/" "$yml"
    fi
    if [ "$rename" = "--rename" ]; then
        info "Renaming package to $LEGACY_PACKAGE (installs next to a newer version)"
        sed -i -E "s/^( +renameManifestPackage:).*/\1 $LEGACY_PACKAGE/" "$yml"
    fi

    info "Rebuilding"
    apktool b -o "$work/unsigned.apk" "$work/decoded"
    "$zipalign" -f -p 4 "$work/unsigned.apk" "$work/aligned.apk"

    local ks="$here/personal.keystore"
    if [ ! -f "$ks" ]; then
        info "Creating a personal signing key ($ks)"
        keytool -genkeypair -keystore "$ks" -storepass android -keypass android -alias personal \
            -keyalg RSA -keysize 2048 -validity 36500 -dname "CN=Personal legacy build" >/dev/null
    fi
    local out="$here/$(basename "${apk%.apk}")-patched.apk"
    "$apksigner" sign --ks "$ks" --ks-pass pass:android --key-pass pass:android --out "$out" "$work/aligned.apk"
    info "Done: $out"
    echo
    echo "Install with:  adb install \"$out\""
    if [ "$rename" != "--rename" ]; then
        echo "If an original copy is installed, uninstall it first (different signature)."
        echo "BACK UP YOUR WORLDS before uninstalling:"
        echo "  adb pull /sdcard/games/com.mojang/minecraftWorlds ./worlds-backup"
    fi
}

[ $# -ge 2 ] || { sed -n '2,16p' "$0"; exit 1; }
[ -f "$2" ] || die "APK not found: $2"
case "$1" in
    check) cmd_check "$2" ;;
    install) cmd_install "$2" ;;
    patch) cmd_patch "$2" "${3:-}" ;;
    *) die "unknown command: $1" ;;
esac
