# NTDB format research

## Current sample

The supplied database starts with the ASCII marker:

    M9DB

A full scan found 12,528 occurrences of the M9P@ record signature.

Observed record section labels:
- ver
- head
- harm
- rres

## Confirmed observations

The head section contains values that are consistent with:
- sample rate
- reference/base frequency
- sample count

One observed record:
- sample rate: 44,100 Hz
- reference frequency: approximately 195.9977 Hz
- sample count: 12,118

The rres section repeatedly shows a header followed by signed 16-bit data whose observed byte count is consistent with the sample-count field.

## Unresolved

The exact record length-prefix rules and section-size encoding are not finalized.

The harm section is not yet semantically decoded. Candidate interpretations include harmonic amplitudes, phases, spectral frames, or a combination.

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
