# Porting and compatibility

## Supported targets

Primary:
- Windows x64 research build
- Android arm64-v8a

Secondary:
- Android x86_64 for emulator/regression testing
- Linux x86_64 for CI and developer tooling

Deferred:
- armeabi-v7a

## API boundary

core/ must not depend on:
- Win32
- Android framework classes
- JNI
- OpenGL
- AudioTrack
- AAudio/Oboe

Platform-specific code belongs outside the portable synthesis/data core.

## Binary parsing

All NTDB fields must be read through explicit little-endian helpers. Do not map the on-disk format by reinterpret_casting packed C++ structs.

The file format is treated as a versioned binary protocol. Unknown fields remain raw bytes until a reference comparison establishes their meaning.

## ABI and CPU

The first implementation is scalar and portable.

ARM64 NEON optimizations may be added later behind a separate implementation layer. The scalar path remains the numerical reference implementation.

Do not assume:
- pointer width
- sizeof(long)
- compiler struct packing
- host endianness
- availability of x86 SSE

## Memory

The supplied NTDB is hundreds of MiB. The research loader may load a complete file into memory for analysis.

The production Android loader should transition to:
- random-access record loading
- lazy decoding
- bounded cache
- shared read-only database mapping where practical

No design should require duplicating the full database for each voice instance.

## Realtime audio

The future render callback must not:
- allocate
- free memory
- perform file I/O
- wait on a mutex
- call Android framework APIs

Audio buffers should be preallocated by the owner of the render graph.

## Version dispatch

The reference DLL exposes multiple decoder/loader generations, including M9P20220801, M9P20221109, M9P20241209, and M9P20250410.

The implementation therefore needs an explicit decoder-version abstraction. Do not silently apply the newest decoder to every database.

## Compatibility goal

The target is behavioral compatibility with the observable data model and synthesis behavior, not PE/DLL binary compatibility.

One C++ core should be buildable as:
- Windows executable/library
- Android arm64-v8a shared library
- Android x86_64 shared library
