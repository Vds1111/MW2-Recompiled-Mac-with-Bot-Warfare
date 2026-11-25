#!/bin/bash
set -e

if [ $# -lt 2 ]; then
    echo "Usage: ./build-mac-app.sh <path/to/mw2.iso OR unpacked_xex_folder> <path/to/TU6_file_or_folder>"
    exit 1
fi

GAME_SRC="$1"
TU6_SRC="$2"

[ -e "$GAME_SRC" ] || { echo "Error: Game source not found: $GAME_SRC"; exit 1; }
[ -e "$TU6_SRC" ]  || { echo "Error: Title Update 6 file/folder not found: $TU6_SRC"; exit 1; }

BUILDER_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="$BUILDER_DIR/.build_work"
APP_OUT="$HOME/Desktop/Modern Warfare 2.app"
mkdir -p "$WORK_DIR"

echo "==> 1. Preparing local source tree..."
if [ ! -d "$WORK_DIR/src" ]; then
    [ -d "$BUILDER_DIR/src" ] || { echo "Error: Local src/ folder missing in $BUILDER_DIR"; exit 1; }
    rsync -a "$BUILDER_DIR/src/" "$WORK_DIR/src/"
fi

SRC="$WORK_DIR/src"

echo "==> 2. Applying Bot Warfare (TU6) patches..."
cp -f "$BUILDER_DIR"/runtime/bots/*.cpp "$SRC/runtime/bots/"
cp -f "$BUILDER_DIR/CMakeLists.txt" "$SRC/"

echo "==> 3. Supplying user's disc executables (ISO or unpacked folder)..."
mkdir -p "$SRC/mw2/tu0"
if [ -d "$GAME_SRC" ]; then
    [ -f "$GAME_SRC/default_mp.xex" ] || { echo "Error: default_mp.xex not found in $GAME_SRC"; exit 1; }
    cp -f "$GAME_SRC/default_mp.xex" "$SRC/mw2/tu0/"
    [ -f "$GAME_SRC/default.xex" ] && cp -f "$GAME_SRC/default.xex" "$SRC/mw2/tu0/"
    ISO_ARG=""
else
    ISO_ARG="ISO=\"$GAME_SRC\""
fi

echo "==> 4. Building TU6 Multiplayer recompiled binary..."
cd "$SRC"
LLVM_BIN="${LLVM:-/usr/bin}"
[ -x "/opt/homebrew/opt/llvm/bin/clang++" ] && LLVM_BIN="/opt/homebrew/opt/llvm/bin"

MVK_ARG="-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/molten-vk;/opt/homebrew/opt/vulkan-loader;/usr/local/opt/molten-vk;/usr/local/opt/vulkan-loader"
for mvk in "$SRC/../deps/moltenvk/MoltenVK/MoltenVK" "$BUILDER_DIR/../deps/moltenvk/MoltenVK/MoltenVK" "/tmp/mvk_shim"; do
    if [ -d "$mvk" ]; then MVK_ARG="$MVK_ARG -DMW2_MOLTENVK=$mvk"; break; fi
done

eval LLVM="$LLVM_BIN" TITLE=mp RELEASE=1 TU="\"$TU6_SRC\"" $ISO_ARG CMAKE_EXTRA="\"$MVK_ARG\"" ./build.sh

echo "==> 5. Packaging standalone Modern Warfare 2.app on Desktop..."
rm -rf "$APP_OUT"
mkdir -p "$APP_OUT/Contents/MacOS" "$APP_OUT/Contents/Resources/game" "$APP_OUT/Contents/Resources/mods" "$APP_OUT/Contents/Resources/saves"

cp -f "$SRC/build-mp-release/mw2" "$APP_OUT/Contents/MacOS/mw2mp"
cp -f "$BUILDER_DIR/assets/launcher" "$APP_OUT/Contents/MacOS/launcher"
[ -f "$BUILDER_DIR/assets/midas_shim.dylib" ] && cp -f "$BUILDER_DIR/assets/midas_shim.dylib" "$APP_OUT/Contents/MacOS/"
chmod +x "$APP_OUT/Contents/MacOS/mw2mp" "$APP_OUT/Contents/MacOS/launcher"

cp -f "$SRC/mw2/default_mp.pe" "$APP_OUT/Contents/Resources/mp_exec"
for ff in patch_mp.ff dlc1_ui_mp.ff dlc2_ui_mp.ff; do
    [ -f "$SRC/mw2/update/$ff" ] && cp -f "$SRC/mw2/update/$ff" "$APP_OUT/Contents/Resources/game/"
done

if [ -d "$GAME_SRC" ]; then
    rsync -a --ignore-existing --exclude='*.xex' "$GAME_SRC/" "$APP_OUT/Contents/Resources/game/"
fi

cp -R "$BUILDER_DIR/mods/bot_warfare" "$APP_OUT/Contents/Resources/mods/"
[ -f "$BUILDER_DIR/assets/mw2.icns" ] && cp -f "$BUILDER_DIR/assets/mw2.icns" "$APP_OUT/Contents/Resources/mw2.icns"

cat << 'PLIST' > "$APP_OUT/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://apple.com">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>launcher</string>
<key>CFBundleIconFile</key><string>mw2.icns</string>
<key>CFBundleIdentifier</key><string>com.mw2.recompiled</string>
<key>CFBundleName</key><string>Modern Warfare 2</string>
<key>CFBundlePackageType</key><string>APPL</string>
</dict></plist>
PLIST

xattr -cr "$APP_OUT"
codesign --force --deep --sign - "$APP_OUT"

echo "==> Done! Your app is ready at: $APP_OUT"
