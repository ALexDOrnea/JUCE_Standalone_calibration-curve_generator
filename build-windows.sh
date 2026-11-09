#!/usr/bin/env bash
# Cross-compiles a Windows 10/11 (x64) .exe from Linux, inside a container with clang-cl.
# The first run downloads the MSVC CRT + Windows SDK from Microsoft via xwin (~1.5 GB),
# which means accepting Microsoft's license for them.
#
# Usage:  ./build-windows.sh [path/to/JUCE]      (default: $JUCE_PATH or ~/JUCE)
# Output: dist/windows/Calibration_Curve_Generator.exe
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JUCE_DIR="$(realpath "${1:-${JUCE_PATH:-$HOME/JUCE}}")"
IMAGE="acoustic-measurer-builder:windows-clangcl"

if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    echo "JUCE not found at $JUCE_DIR. Pass the path as the first argument." >&2
    exit 1
fi

if command -v podman >/dev/null; then
    ENGINE=podman
    RUN_AS=(--userns=keep-id)
elif command -v docker >/dev/null; then
    ENGINE=docker
    RUN_AS=(--user "$(id -u):$(id -g)")
else
    echo "Neither podman nor docker is installed. On Arch: sudo pacman -S podman" >&2
    exit 1
fi

"$ENGINE" build -t "$IMAGE" -f "$PROJECT_DIR/packaging/windows/Containerfile" "$PROJECT_DIR/packaging/windows"

"$ENGINE" run --rm "${RUN_AS[@]}" \
    -v "$PROJECT_DIR":/src:Z \
    -v "$JUCE_DIR":/juce:ro,Z \
    -w /src \
    "$IMAGE" bash -euo pipefail -c '
        # JUCE asks for e.g. "Dwrite.lib" via #pragma comment(lib); the SDK has dwrite.lib/DWRITE.lib and
        # Linux filenames are case-sensitive. Link every requested name to its case-insensitive match.
        CASE_DIR=/tmp/xwin-case; mkdir -p "$CASE_DIR"
        LIB_DIRS="/xwin/crt/lib/x86_64 /xwin/sdk/lib/um/x86_64 /xwin/sdk/lib/ucrt/x86_64"
        grep -rhoE "pragma comment *\( *lib *, *\"[^\"]+\"" /juce/modules | sed -E "s/.*\"([^\"]+)\"/\1/" | sort -u |
        while read -r name; do
            [[ "$name" == *.* ]] || name="$name.lib"
            found=""
            for d in $LIB_DIRS; do [[ -e "$d/$name" ]] && found=1 && break; done
            [[ -n "$found" ]] && continue
            match=$(find $LIB_DIRS -maxdepth 1 -iname "$name" -print -quit)
            [[ -n "$match" ]] && ln -sf "$match" "$CASE_DIR/$name"
        done
        cmake -S . -B build-windows -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE=/opt/clang-cl-toolchain.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DJUCE_PATH=/juce \
            -DCALIB_BUILD_TESTS=OFF
        cmake --build build-windows -j"$(nproc)"
    '

EXE="$(find "$PROJECT_DIR/build-windows/CalibrationCurveGenerator_artefacts" -name '*.exe' -print -quit)"
mkdir -p "$PROJECT_DIR/dist/windows"
cp "$EXE" "$PROJECT_DIR/dist/windows/"

echo
echo "Done: dist/windows/$(basename "$EXE")"
echo "DLLs it imports (should all be part of Windows itself):"
objdump -p "$EXE" 2>/dev/null | awk '/DLL Name/ {print "  " $3}' | sort -u || true
