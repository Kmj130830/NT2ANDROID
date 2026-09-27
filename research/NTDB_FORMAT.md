# NTDB format research

## Current sample

The supplied database starts with the ASCII marker:

    M9DB

A full scan found 12,528 occurrences of the M9P@ record signature.

The records use the following observed structure:

    M9P@ + 0x14-byte prefix
    head tag + 24-byte payload
    harm tag + variable payload
    rres tag + variable payload

The final M9P record is not necessarily the end of the NTDB file. The record declares its own body size, and the database contains additional trailing data after the final M9P record.

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

For normal records, the body-size field satisfies:

    record_size = 16 + bodySize

The final record is a useful confirmation: its declared body size is 43,374 bytes, giving a record end at 0x1AADF625. The NTDB file itself continues for several MiB after that position, so the trailing bytes are database-level data rather than part of the final M9P record.

## Version field

The version payload in the majority of records is:

    2a ff 34 01

Interpreted as little-endian uint32:

    0x0134ff2a = 20250410

The database contains 12,523 records with version 20250410 and 5 records with version 20241209. Those five older-format records occur contiguously at record indices 5953 through 5957.

Both values correspond to decoder/loader generations observed in the reference Windows DLL.

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

The previous assumption that offset 0x08 was sample rate was incorrect. It is 7.172912598 in every displayed record, and the inspector reports exactly one distinct value across the database.

The reference-frequency field is strong evidence: treating bytes at head+0x0C as an IEEE-754 little-endian double gives musical frequencies including:

    195.9977179908746 Hz
    261.6255653005986 Hz
    329.6275569128700 Hz
    391.9954359817493 Hz
    523.2511306 Hz

The source sample rate is not identified by this field.

## rres

The observed rres payload has a strong size relationship:

    rres_payload_size = 16 + sampleCount*2 + 8

for almost all records inspected. The final-record boundary must use the M9P record's declared size rather than the physical end of the NTDB file.

The first 16 bytes of a typical rres payload are:

    uint32 storedSize
    uint32 zero/reserved
    uint32 sampleCount
    uint32 sampleCount

For record 0:

    storedSize  = 24,248
    sampleCount = 12,118
    rres payload = 24,260

and:

    storedSize + 12 = rres payload size

The candidate residual region is:

    offset +0x10
    sampleCount signed 16-bit values
    then an 8-byte trailer

The residual/trailer boundary is therefore:

    +0x10 through +0x10 + sampleCount*2
    trailer immediately after

The exact semantics of storedSize, the duplicate sampleCount fields, and the 8-byte trailer are not yet confirmed.

## harm

The harm section has a variable payload length and occurs between head and rres.

Its semantic representation is not yet confirmed. It is likely analysis/spectral information, but the exact element size, frame structure, scaling, and meaning must be established from repeated patterns and reference behavior.

## Research discipline

A byte pattern is not considered a field definition merely because its value looks plausible.

A field becomes confirmed when:
1. its boundaries are reproducible across records,
2. its interpretation agrees with multiple records or a reference output,
3. the parser can round-trip or otherwise cross-check it.
