#!/bin/bash
# macOS dependency bundling using dylibbundler
# This script uses dylibbundler to automatically copy dependencies and fix install_name references

set -e  # Exit on error

echo "[bundle-macos] Starting macOS dependency bundling with dylibbundler..."

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
  PLATFORM="darwin-x64"
  HOMEBREW_PREFIX="/usr/local"
elif [ "$ARCH" = "arm64" ]; then
  PLATFORM="darwin-arm64"
  HOMEBREW_PREFIX="/opt/homebrew"
else
  echo "[bundle-macos] ❌ Unsupported architecture: $ARCH"
  exit 1
fi

echo "[bundle-macos] Platform: $PLATFORM"
echo "[bundle-macos] Homebrew prefix: $HOMEBREW_PREFIX"

# Target directory
TARGET_DIR="prebuilds/$PLATFORM"
NODE_FILE="$TARGET_DIR/node.napi.node"

# Check if target directory and binary exist
if [ ! -d "$TARGET_DIR" ]; then
  echo "[bundle-macos] ❌ Target directory not found: $TARGET_DIR"
  echo "[bundle-macos] Run 'npm run prebuild' first"
  exit 1
fi

if [ ! -f "$NODE_FILE" ]; then
  echo "[bundle-macos] ❌ Binary not found: $NODE_FILE"
  echo "[bundle-macos] Run 'npm run prebuild' first"
  exit 1
fi

# Check if dylibbundler is installed
if ! command -v dylibbundler &> /dev/null; then
  echo "[bundle-macos] ❌ dylibbundler not found"
  echo "[bundle-macos] Install with: brew install dylibbundler"
  exit 1
fi

echo "[bundle-macos] Using dylibbundler: $(which dylibbundler)"

# Search paths for dependencies
# Priority 1: HAMLIB_PREFIX if set (for custom builds)
# Priority 2: Homebrew locations
SEARCH_PATHS=()

if [ -n "$HAMLIB_PREFIX" ]; then
  echo "[bundle-macos] Using HAMLIB_PREFIX: $HAMLIB_PREFIX"
  SEARCH_PATHS+=("$HAMLIB_PREFIX/lib")
fi

SEARCH_PATHS+=(
  "$HOMEBREW_PREFIX/opt/hamlib/lib"
  "$HOMEBREW_PREFIX/opt/fftw/lib"
  "$HOMEBREW_PREFIX/opt/gcc/lib/gcc/current"
  "$HOMEBREW_PREFIX/opt/gcc@14/lib/gcc/14"
  "$HOMEBREW_PREFIX/opt/gcc@13/lib/gcc/13"
  "$HOMEBREW_PREFIX/lib"
)

# Build dylibbundler command with all search paths
DYLIBBUNDLER_CMD="dylibbundler -x \"$NODE_FILE\" -d \"$TARGET_DIR\" -p \"@loader_path/\""

for search_path in "${SEARCH_PATHS[@]}"; do
  if [ -d "$search_path" ]; then
    DYLIBBUNDLER_CMD="$DYLIBBUNDLER_CMD -s \"$search_path\""
    echo "[bundle-macos] Adding search path: $search_path"
  fi
done

# Add -b flag to create missing directories and overwrite existing files
DYLIBBUNDLER_CMD="$DYLIBBUNDLER_CMD -b"

echo "[bundle-macos] Running dylibbundler..."
echo "[bundle-macos] Command: $DYLIBBUNDLER_CMD"

# Execute dylibbundler
eval $DYLIBBUNDLER_CMD

echo ""
echo "[bundle-macos] ✅ Bundling completed successfully"
echo ""

# Verify results with otool
echo "[bundle-macos] Verification: Checking dependencies with otool -L"
echo "[bundle-macos] ================================================"
otool -L "$NODE_FILE"
echo ""

# List all bundled dylibs
echo "[bundle-macos] Bundled libraries in $TARGET_DIR:"
ls -lh "$TARGET_DIR"/*.dylib 2>/dev/null || echo "[bundle-macos] No additional dylibs bundled (all system libraries)"
echo ""

# Verify each bundled dylib
DYLIB_FILES=("$TARGET_DIR"/*.dylib)
if [ -e "${DYLIB_FILES[0]}" ]; then
  echo "[bundle-macos] Verifying bundled libraries:"
  for dylib in "$TARGET_DIR"/*.dylib; do
    echo "[bundle-macos] ------------------------------------------------"
    echo "[bundle-macos] $(basename "$dylib"):"
    otool -L "$dylib" | grep -v ":" | grep "@loader_path" || echo "  (no @loader_path references or all system libs)"
  done
fi

echo ""
echo "[bundle-macos] ================================================"
echo "[bundle-macos] ✅ All done! Verify that all references use @loader_path/"
