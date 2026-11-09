# JUCE_Standalone_calibration-curve_generator

C++/JUCE port of `eq_generator.py`: reads a measurement CSV (frequency, script value, reference value),
interpolates the error with PCHIP on a log-frequency axis and exports a calibration file.

## Build (this machine)

* `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
* `cmake --build build -j`
* tests: `build/CalibrationCurveGeneratorTests_artefacts/Release/CalibrationCurveGeneratorTests`

Binary: `build/CalibrationCurveGenerator_artefacts/Release/Calibration_Curve_Generator`

## Portable Linux build (glibc >= 2.35: Ubuntu 22.04+, Debian 12+, Fedora 36+, Mint 21+, ...)

* install podman (or docker), then `./build-linux-portable.sh` (optionally pass the JUCE path)

Output: `dist/linux/Calibration_Curve_Generator`

## Windows 10/11 build (cross-compiled from Linux with clang-cl + xwin)

* install podman (or docker), then `./build-windows.sh` (optionally pass the JUCE path)

Output: `dist/windows/Calibration_Curve_Generator.exe`, a single x64 .exe with a static runtime
(no Visual C++ Redistributable needed). It is not code-signed: on first launch SmartScreen may
show "Windows protected your PC" -> "More info" -> "Run anyway".
