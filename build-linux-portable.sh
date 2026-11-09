#!/usr/bin/env bash
# Builds a Linux binary that runs on most distributions (glibc >= 2.35), not just this machine.
# The build happens inside an Ubuntu 22.04 container; your normal build/ folder is not touched.
#
# Usage:  ./build-linux-portable.sh [path/to/JUCE]      (default: $JUCE_PATH or ~/JUCE)
# Output: dist/linux/Calibration_Curve_Generator
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JUCE_DIR="$(realpath "${1:-${JUCE_PATH:-$HOME/JUCE}}")"
IMAGE="acoustic-measurer-builder:ubuntu22.04"

if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    echo "JUCE not found at $JUCE_DIR. Pass the path as the first argument." >&2
    exit 1
fi

if command -v podman >/dev/null; then
    ENGINE=podman
    RUN_AS=(--userns=keep-id)            # files created in the container belong to you
elif command -v docker >/dev/null; then
    ENGINE=docker
    RUN_AS=(--user "$(id -u):$(id -g)")
else
    echo "Neither podman nor docker is installed. On Arch: sudo pacman -S podman" >&2
    exit 1
fi

# The image is built once and cached; later runs start immediately.
"$ENGINE" build -t "$IMAGE" -f "$PROJECT_DIR/packaging/linux/Containerfile" "$PROJECT_DIR/packaging/linux"

"$ENGINE" run --rm "${RUN_AS[@]}" \
    -v "$PROJECT_DIR":/src:Z \
    -v "$JUCE_DIR":/juce:ro,Z \
    -w /src \
    "$IMAGE" bash -euo pipefail -c '
        cmake -S . -B build-portable -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DJUCE_PATH=/juce \
            -DCALIB_BUILD_TESTS=OFF \
            -DCALIB_PORTABLE_LINUX=ON
        cmake --build build-portable -j"$(nproc)"
    '

BIN="$PROJECT_DIR/build-portable/CalibrationCurveGenerator_artefacts/Release/Calibration_Curve_Generator"
mkdir -p "$PROJECT_DIR/dist/linux"
cp "$BIN" "$PROJECT_DIR/dist/linux/"
strip "$PROJECT_DIR/dist/linux/Calibration_Curve_Generator"

echo
echo "Done: dist/linux/Calibration_Curve_Generator"
echo -n "Newest glibc symbol required: "
objdump -T "$PROJECT_DIR/dist/linux/Calibration_Curve_Generator" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1
echo "Shared libraries needed (present on any desktop Linux):"
objdump -p "$PROJECT_DIR/dist/linux/Calibration_Curve_Generator" | awk '/NEEDED/ {print "  " $2}'
