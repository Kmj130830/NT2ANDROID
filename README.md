# NT2ANDROID

Experimental native reimplementation/research project for an NT2-era vocal synthesis pipeline.

The Windows x64 PPS_NT2.dll is treated as a reference binary, not as a build dependency.

## Architecture

The project is split into:
- portable C++17 core
- Windows research/test tools
- Android NDK JNI/audio adapters
- Android UI layer

The same portable core is intended to build on Windows x64, Android arm64-v8a, and Android x86_64.

## Compatibility targets

- Primary Android ABI: arm64-v8a
- Secondary ABI: x86_64
- Initial Android API: 27
- C++ standard: C++17
- 32-bit armeabi-v7a: deferred
- SIMD: scalar reference first; optional ARM64 NEON later

## Current status

M0 — NTDB structural reconnaissance.

Confirmed in the supplied sample outside this repository:
- M9DB file marker
- 12,528 M9P records
- per-record sections: ver, head, harm, rres
- head contains values consistent with sample rate, reference frequency, and sample count
- rres contains a header plus signed 16-bit residual data
- PPS_NT2.dll contains M9Database, M9Decoder, M9Engine, M9EngineSynthesizer and multiple M9P loader/decoder version names

The exact meanings of every binary field are not assumed until independently reproduced.

## Windows build

Requirements:
- CMake 3.21+
- C++17 compiler

    cmake -S . -B build-windows -DNT2_BUILD_TESTS=ON
    cmake --build build-windows --config Release

## Android build

Requirements:
- Android SDK
- Android NDK
- NDK CMake toolchain

    cmake -S . -B build-android -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-27 -DNT2_BUILD_TESTS=OFF
    cmake --build build-android --config Release

The first Android milestone is a loadable libnt2core.so.

## Local proprietary files

Do not add NTDB databases, PPS_NT2.dll, Piapro executables, license files, or other proprietary binaries to this repository.

For local research:

    nt2db_inspect.exe "C:\NT2Research\MIKU NT Original.ntdb"

## Design rules

- No Android framework dependency in core/
- No Win32 dependency in core/
- Explicit little-endian parsing
- Do not cast binary data directly onto packed C++ structs
- Avoid hidden global state
- Realtime audio code must not allocate, block on I/O, lock, or call framework APIs
- Production NTDB loading should eventually use lazy record access instead of duplicating the whole database in RAM
