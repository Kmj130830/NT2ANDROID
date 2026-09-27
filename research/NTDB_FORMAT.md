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

The bytes before "head" are therefore a fixed 20-byte per-record prefix for this database. Its internal "ver" encoding is not yet decoded.

## head payload

"head" has a 24-byte payload.

Observed byte layout:

| Offset | Size | Current interpretation |
|---:|---:|---|
| 0x00 | 4 | declared size; observed value 20 |
| 0x04 | 4 | flags/reserved; observed value 0 |
| 0x08 | 4 | IEEE-754 little-endian float sample rate |
| 0x0C | 8 | IEEE-754 little-endian double reference frequency |
| 0x14 | 4 | unsigned sample count |

Examples from the supplied database:

    44,100.0 Hz
    195.9977179908746 Hz
    12,118 samples

The reference-frequency field is especially strong evidence: treating bytes at head+0x0C as an IEEE-754 little-endian double gives exact musical base frequencies in multiple records, including approximately:

    195.9977179908746 Hz
    261.6255653005986 Hz
    329.6275569128700 Hz
    391.9954359817493 Hz

These correspond to expected equal-temperament note frequencies, which independently supports the field interpretation.

## rres

For tested records, the rres payload consists of:

    24-byte header
    followed by signed 16-bit residual samples

The residual byte count agrees with:

    sampleCount * 2

after subtracting the 24-byte rres header.

For the first record:

    sampleCount = 12,118
    residual samples = 12,118
    residual bytes = 24,236
    rres payload = 24,260 bytes
    rres header = 24 bytes

The 24-byte rres header field meanings are not yet decoded.

## harm

The harm section has a variable payload length and occurs between head and rres.

Its semantic representation is not yet confirmed. The current evidence suggests it is likely analysis/spectral data, but the exact element size, frame structure, scaling, and meaning must be established from repeated byte patterns and reference behavior.

## Versioning evidence

The reference Windows DLL contains multiple M9P decoder/loader names:
- M9P20220801
- M9P20221109
- M9P20241209
- M9P20250410

The portable parser must therefore retain version information and dispatch to the appropriate decoder.

## Research discipline

A byte pattern is not considered a field definition merely because its value looks plausible.

A field becomes confirmed when:
1. its boundaries are reproducible across records,
2. its interpretation agrees with multiple records or a reference output,
3. the parser can round-trip or otherwise cross-check it.
