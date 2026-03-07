#!/bin/bash
# wasm_tools/build.sh
# Owner: Dev B
# Compiles all C++ tools to WebAssembly (.wasm) using wasi-sdk.

set -e

# Default to /tmp/wasi-sdk if WASI_SDK_PATH is not set
export WASI_SDK_PATH="${WASI_SDK_PATH:-/tmp/wasi-sdk-24.0}"
WASI_SDK_URL="https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/wasi-sdk-24.0-x86_64-linux.tar.gz"

echo "=== Wasm Tools Build Script ==="

# Download wasi-sdk if not present
if [ ! -d "$WASI_SDK_PATH" ]; then
    echo "wasi-sdk not found at $WASI_SDK_PATH. Downloading..."
    mkdir -p /tmp/wasi-sdk-download
    cd /tmp/wasi-sdk-download
    curl -L -O "$WASI_SDK_URL"
    echo "Extracting wasi-sdk..."
    tar xzf wasi-sdk-24.0-x86_64-linux.tar.gz -C /tmp
    # The tarball extracts a folder named wasi-sdk-24.0-x86_64-linux
    if [ -d "/tmp/wasi-sdk-24.0-x86_64-linux" ] && [ "/tmp/wasi-sdk-24.0-x86_64-linux" != "$WASI_SDK_PATH" ]; then
        mv /tmp/wasi-sdk-24.0-x86_64-linux "$WASI_SDK_PATH"
    fi
    cd - > /dev/null
    echo "wasi-sdk installed to $WASI_SDK_PATH"
fi

CXX="$WASI_SDK_PATH/bin/clang++"

if [ ! -x "$CXX" ]; then
    echo "Error: wasi-sdk clang++ not found at $CXX"
    exit 1
fi

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$TOOLS_DIR/src"

echo "Compiling C++ tools to WebAssembly..."

# Compilation flags
# -O3 for optimization
# -s to strip binaries
# -fno-exceptions because WASI doesn't support C++ exceptions by default
CXXFLAGS="-O3 -s -fno-exceptions"

for src_file in "$SRC_DIR"/*.cpp; do
    if [ ! -f "$src_file" ]; then continue; fi
    
    filename=$(basename "$src_file")
    tool_name="${filename%.cpp}"
    out_wasm="$TOOLS_DIR/${tool_name}.wasm"
    
    echo "  Compiling $tool_name..."
    "$CXX" $CXXFLAGS "$src_file" -o "$out_wasm"
done

echo "=== Build Complete ==="
echo "Tools compiled to $TOOLS_DIR/*.wasm"
