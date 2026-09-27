# NTDB format research

## Current sample

The supplied database starts with the ASCII marker:

    M9DB

A full scan found 12,528 occurrences of the M9P@ record signature.

Every one of the 12,528 records currently matches the observed structural layout:

    M9P@ + 0x14-byte prefix
    head tag + 24-byte payload
    harm tag + variable payload
    rres tag + variable payload
    next M9P@

Observed record section labels:
- ver/prefix region
- head
- harm
- rres

## M9P record prefix

The first "head" tag occurs exactly 0x14 (20) bytes after the M9P@ signature in all 12,528 records tested.

The observed prefix layout is:

| Relative offset | Size | Current interpretation |
|---:|---:|---|
| 0x00 | 4 | ASCII M9P@ |
| 0x04 | 4 | uint32 body size |
| 0x08 | 4 | ASCII ver + space |
| 0x0C | 4 | uint32 version payload size; observed 4 |
| 0x10 | 4 | uint32 M9P format version |

The body-size field matches:

    record_size - 16

for the records inspected.

The version payload is:

    2a ff 34 01

Interpreted as little-endian uint32:

    0x0134ff2a = 20250410

This exactly matches the M9P20250410 loader/decoder generation observed in the reference Windows DLL. This is strong evidence that the supplied database uses the M9P20250410 record format.

## head payload

head has a 24-byte payload.

Observed byte layout:

| Offset | Size | Current interpretation |
|---:|---:|---|
| 0x00 | 4 | declared size; observed value 20 |
| 0x04 | 4 | flags/reserved; observed value 0 |
| 0x08 | 4 | unknown little-endian IEEE-754 float; observed value 7.172912598 |
| 0x0C | 8 | IEEE-754 little-endian double reference frequency |
| 0x14 | 4 | unsigned sample count |

The previous assumption that offset 0x08 was sample rate was incorrect. It is 7.172912598 in the displayed records, not 44,100 Hz.

The reference-frequency field is especially strong evidence: treating bytes at head+0x0C as an IEEE-754 little-endian double gives exact musical base frequencies in multiple records, including approximately:

    195.9977179908746 Hz
    261.6255653005986 Hz
    329.6275569128700 Hz
    391.9954359817493 Hz
    523.2511306 Hz

The source sample rate is not yet located in this per-record field and should not be assumed to be the value at offset 0x08.

## rres

The current first-24-byte observations do NOT justify calling the entire 24 bytes a header.

Examples:

    rres payload = 24,260 bytes
    sampleCount = 12,118
    sampleCount * 2 = 24,236 bytes

The first four rres payload bytes are:

    b8 5e 00 00

which is 24,248 as little-endian uint32, i.e.:

    residual_bytes + 12

The next fields observed in the first record are:

    +0x00 : 24248
    +0x04 : 0
    +0x08 : 12118
    +0x0C : 12118
    +0x10 : f8 ff 1b 00 0d 00 f5 ff ...

The values at +0x10 onward look consistent with signed 16-bit residual samples.

There is therefore a strong candidate layout of:

    16-byte leading rres metadata
    sampleCount signed 16-bit residual values
    8 trailing bytes

but the exact meaning of the leading size field and the trailing 8 bytes is not yet confirmed.

The next inspection step prints both the first 16 bytes and the last 16 bytes of rres and verifies:

    16 + sampleCount*2 + 8 == rres_payload_size

before assigning semantic names.

## harm

The harm section has a variable payload length and occurs between head and rres.

Its semantic representation is not yet confirmed. The current evidence suggests it is likely analysis/spectral data, but the exact element size, frame structure, scaling, and meaning must be established from repeated byte patterns and reference behavior.

## Versioning evidence

The reference Windows DLL contains multiple M9P decoder/loader names:
- M9P20220801
- M9P20221109
- M9P20241209
- M9P20250410

The supplied database's per-record version field is 20250410, matching the newest observed decoder generation.

## Research discipline

A byte pattern is not considered a field definition merely because its value looks plausible.

A field becomes confirmed when:
1. its boundaries are reproducible across records,
2. its interpretation agrees with multiple records or a reference output,
3. the parser can round-trip or otherwise cross-check it.
