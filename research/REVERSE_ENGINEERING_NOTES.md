# Reverse engineering notes

The supplied Windows distribution contains a large PPS_NT2.dll.

Observed symbol/RTTI/string evidence includes:
- M9Engine
- IM9Engine
- M9Decoder
- IM9Decoder
- M9DatabaseWrapper
- IM9DatabaseWrapper
- M9EngineSynthesizer
- SimpleM9PLoader
- Decoder@M9P20220801
- Decoder@M9P20221109
- Decoder@M9P20241209
- SimpleM9PLoader@M9P20250410
- FFT
- DCT
- PhonemeClassifier
- F0Flattener
- M9Database
- M9DatabaseBody
- getDatabaseInfo
- NTDB to load
- NTDB already loaded
- NTDB Unloaded

These names support a working architectural hypothesis:

editor/project
    -> database wrapper
    -> M9P loader/decoder
    -> synthesis engine

Names found in a native binary are evidence of implementation concepts, not proof of exact algorithms.

The repository therefore keeps the implementation independent from undocumented assumptions and records hypotheses separately from confirmed layout information.
