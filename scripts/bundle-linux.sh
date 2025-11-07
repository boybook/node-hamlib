#!/bin/bash
# Linux dependency bundling using ldd + patchelf
# This script copies non-system dependencies and sets RUNPATH to $ORIGIN

set -e  # Exit on error

echo "[bundle-linux] Starting Linux dependency bundling..."

# Detect architecture
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
  PLATFORM="linux-x64"
elif [ "$ARCH" = "aarch64" ]; then
  PLATFORM="linux-arm64"
else
  echo "[bundle-linux] ❌ Unsupported architecture: $ARCH"
  exit 1
fi

echo "[bundle-linux] Platform: $PLATFORM"

# Target directory
TARGET_DIR="prebuilds/$PLATFORM"
NODE_FILE="$TARGET_DIR/node.napi.node"

# Check if target directory and binary exist
if [ ! -d "$TARGET_DIR" ]; then
  echo "[bundle-linux] ❌ Target directory not found: $TARGET_DIR"
  echo "[bundle-linux] Run 'npm run prebuild' first"
  exit 1
fi

if [ ! -f "$NODE_FILE" ]; then
  echo "[bundle-linux] ❌ Binary not found: $NODE_FILE"
  echo "[bundle-linux] Run 'npm run prebuild' first"
  exit 1
fi

# Check if patchelf is installed
if ! command -v patchelf &> /dev/null; then
  echo "[bundle-linux] ❌ patchelf not found"
  echo "[bundle-linux] Install with: sudo apt-get install patchelf"
  exit 1
fi

echo "[bundle-linux] Using patchelf: $(which patchelf)"

# Find libhamlib.so
echo "[bundle-linux] Searching for libhamlib.so..."
HAMLIB_LIB=""

# Priority 1: Check HAMLIB_PREFIX environment variable
if [ -n "$HAMLIB_PREFIX" ]; then
  echo "[bundle-linux] Checking HAMLIB_PREFIX: $HAMLIB_PREFIX"
  for path in "$HAMLIB_PREFIX/lib/libhamlib.so.4" "$HAMLIB_PREFIX/lib/libhamlib.so"; do
    if [ -f "$path" ]; then
      HAMLIB_LIB="$path"
      echo "[bundle-linux] Found libhamlib in HAMLIB_PREFIX: $path"
      break
    fi
  done
fi

# Priority 2: Try ldconfig
if [ -z "$HAMLIB_LIB" ] && command -v ldconfig &> /dev/null; then
  HAMLIB_LIB=$(ldconfig -p | grep 'libhamlib\.so' | head -1 | awk '{print $NF}')
fi

# Priority 3: Fallback to common system paths
if [ -z "$HAMLIB_LIB" ]; then
  for path in /usr/lib64/libhamlib.so.4 /usr/lib/libhamlib.so.4 /usr/local/lib/libhamlib.so.4 \
              /usr/lib/x86_64-linux-gnu/libhamlib.so.4 /usr/lib/aarch64-linux-gnu/libhamlib.so.4 \
              /usr/lib64/libhamlib.so /usr/lib/libhamlib.so /usr/local/lib/libhamlib.so; do
    if [ -f "$path" ]; then
      HAMLIB_LIB="$path"
      break
    fi
  done
fi

if [ -z "$HAMLIB_LIB" ]; then
  echo "[bundle-linux] ❌ libhamlib.so not found"
  echo "[bundle-linux] Install hamlib development package"
  exit 1
fi

echo "[bundle-linux] Found libhamlib: $HAMLIB_LIB"

# Copy libhamlib.so
HAMLIB_BASENAME=$(basename "$HAMLIB_LIB")
cp "$HAMLIB_LIB" "$TARGET_DIR/"
echo "[bundle-linux] ✓ Copied $HAMLIB_BASENAME to $TARGET_DIR/"

# Find and copy transitive dependencies
echo "[bundle-linux] Analyzing dependencies with ldd..."
echo ""

# System libraries that should NOT be bundled
SYSTEM_LIBS_PATTERN="linux-vdso|ld-linux|libc\.so|libm\.so|libpthread\.so|libdl\.so|librt\.so|libgcc_s\.so|libstdc\+\+\.so"

# Get dependencies excluding system libraries
DEPS=$(ldd "$HAMLIB_LIB" | grep "=>" | grep -vE "$SYSTEM_LIBS_PATTERN" | awk '{print $1 " " $3}')

if [ -z "$DEPS" ]; then
  echo "[bundle-linux] No additional dependencies found (or all are system libraries)"
else
  echo "[bundle-linux] Found non-system dependencies:"
  echo "$DEPS" | while read -r libname libpath; do
    echo "[bundle-linux]   - $libname (from $libpath)"
    if [ -f "$libpath" ] && [ "$libpath" != "not" ]; then
      cp "$libpath" "$TARGET_DIR/" 2>/dev/null || echo "[bundle-linux]     ⚠ Failed to copy $libname"
      echo "[bundle-linux]     ✓ Bundled to $TARGET_DIR/$(basename "$libpath")"
    fi
  done
fi

echo ""

# Set RUNPATH for all .so files and .node file
echo "[bundle-linux] Setting RUNPATH=\$ORIGIN for all binaries..."

for file in "$TARGET_DIR"/*.node "$TARGET_DIR"/*.so "$TARGET_DIR"/*.so.*; do
  if [ -f "$file" ]; then
    filename=$(basename "$file")
    patchelf --set-rpath '$ORIGIN' "$file" 2>/dev/null && \
      echo "[bundle-linux]   ✓ Set RUNPATH for $filename" || \
      echo "[bundle-linux]   ⚠ Failed to set RUNPATH for $filename"
  fi
done

echo ""
echo "[bundle-linux] ✅ Bundling completed successfully"
echo ""

# Verify results with ldd
echo "[bundle-linux] Verification: Checking dependencies with ldd"
echo "[bundle-linux] ================================================"
echo "[bundle-linux] node.napi.node dependencies:"
ldd "$NODE_FILE" || true
echo ""

# Check for missing dependencies
MISSING=$(ldd "$NODE_FILE" 2>&1 | grep "not found" || true)
if [ -n "$MISSING" ]; then
  echo "[bundle-linux] ❌ Missing dependencies detected:"
  echo "$MISSING"
  exit 1
else
  echo "[bundle-linux] ✅ All dependencies resolved"
fi

echo ""

# Verify RUNPATH
echo "[bundle-linux] Verification: Checking RUNPATH"
echo "[bundle-linux] ================================================"
for file in "$TARGET_DIR"/*.node "$TARGET_DIR"/*.so "$TARGET_DIR"/*.so.*; do
  if [ -f "$file" ]; then
    filename=$(basename "$file")
    rpath=$(patchelf --print-rpath "$file" 2>/dev/null || echo "(none)")
    echo "[bundle-linux] $filename: RUNPATH=$rpath"
  fi
done

echo ""

# List bundled files
echo "[bundle-linux] Bundled files in $TARGET_DIR:"
ls -lh "$TARGET_DIR"/*.so* 2>/dev/null || echo "[bundle-linux] (none)"
echo ""
echo "[bundle-linux] ================================================"
echo "[bundle-linux] ✅ All done!"
