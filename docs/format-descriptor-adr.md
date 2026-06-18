# ADR-001: DIY data-driven format descriptor (not a formal binary grammar)

- Status: **Accepted** (2026-06-18)
- Beads: decision ddm-3j8.1.7.4; strategy epic ddm-3j8.1.7; codec ddm-3j8.1.1;
  version-robustness design ddm-3j8.1.7.3.

## Context

We must decode DUNE DAQ packed ADC formats (WIBEth 14-bit first) and defend
against **silent format drift** (DAQ layouts change, sometimes without bumping
embedded version fields). Two options were weighed:

- **(A) A project-owned, data-driven format descriptor** parameterizing one
  generic bit-unpack loop.
- **(B) A formal binary-grammar tool**, principally **Kaitai Struct** (`.ksy` →
  generated C++; bit-sized ints, `valid:` constraints).

Findings (beads ddm-3j8.1.7.1/.7.2):

- The packing is **flat and regular** — LSB-first N-bit fields in little-endian
  64-bit words (see `wibeth-format.md`); the reference extraction is ~25 lines,
  already parameterized by `(word_bits, bits_per_adc, n_channels, n_samples)`.
- Other binary-grammar tools don't fit: DFDL/Daffodil is Java-heavy with no light
  C++ runtime; Construct/Hachoir are Python; Protobuf/FlatBuffers/Cap'n Proto
  define *their own* serialization and cannot describe an externally-fixed
  bit-packing. The reference repos ship **no** machine-readable grammar; the
  canonical spec is EDMS 2088713 (prose) plus the C++ structs.
- Kaitai would add a build dependency (JVM compiler, or committed generated
  code) + a runtime lib, and — critically — reads **bit-by-bit, materializing
  per-element objects**, a real performance risk in the bulk-ADC hot path
  (64×64 ADC/frame, millions/event).

## Decision

Adopt **(A)**: a small, project-owned, **data-driven descriptor** that
parameterizes a single generic decode loop. Reasons:

- It matches the regularity of the packing — Kaitai's expressiveness buys little
  here.
- **Zero external dependency** (consistent with the project's narrow-waist,
  dependency-minimal style and the `reference/`-is-read-only rule).
- Full control of the hot loop (vectorizable unpack), no JVM/codegen step.
- A *descriptor as data* gives the property we most want: **new format versions
  can be added without rebuilding the decode code**.

Kaitai is retained as a **documented fallback** if a future detector
(DAPHNE/PDS, trigger primitives) turns out to have a materially irregular or
variable-length packing that the descriptor cannot express cleanly. (We have no
experience with those detectors yet; the DIY risk is accepted.)

## The descriptor (design sketch)

A descriptor is plain data — one record per (detector, format version):

```
FormatDescriptor {
  id            : { fragment_type, daq_eth_version, wib_version }  // how it is recognized
  word_bits     : 64
  bits_per_adc  : 14
  n_channels    : 64
  n_samples     : 64           // time ticks per frame
  frame_header_bytes : 32      // DAQEthHeader(16) + WIBEthHeader(16)
  adc_offset_bytes   : 32      // where the packed ADC block starts in a frame
  frame_bytes        : 7200    // total fixed frame size (derived; also checked)
  header_fields : [ {name, word, lsb, width} ... ]  // bitfield map for metadata
  invariants    : [ ... ]      // see below
}
```

One generic routine unpacks LSB-first `bits_per_adc`-bit fields from
`word_bits`-wide little-endian words into a dense `int16[n_channels][n_samples]`
block; the header-field map extracts metadata. Adding a new version = adding a
descriptor record (data), not new code. Descriptors may be compiled-in tables
initially; a future iteration can load them from config to make new formats
**rebuild-free** (the original motivation).

## Version-robustness strategy (detail in ddm-3j8.1.7.3)

- **Declared version** comes from `DAQEthHeader.version`, `WIBEthHeader.version`,
  and `FragmentType`; treat these as advisory (they may not change when the
  layout silently does).
- **Minimum bar (must):** for the selected descriptor, enforce its `invariants`
  and **fail loudly** on violation. Candidate invariants: `payload % frame_bytes
  == 0`; reserved/pad bits zero; `det_id`/`crate`/`slot`/`stream` in range; ADC
  field width matches; timestamps non-decreasing across a stream; declared sizes
  consistent with byte counts.
- **Stretch (best):** **auto-detect** the actual layout by trial-parse — run the
  candidate descriptors and accept the unique one whose invariants hold; error on
  none/ambiguous. Inherently heuristic; document its limits.

## Consequences

- We own the decode loop and its tests (validated against a pinned reference
  fragment with known ADC values — to be captured with ddm-3j8.1.2).
- Per-detector parameters must be transcribed carefully from the reference and
  EDMS docs (a transcription-error risk Kaitai would not have removed, since its
  `.ksy` would need the same transcription).
- If irregular detectors arrive, revisit (A) vs (B) for those specific formats;
  the descriptor and a Kaitai parser could coexist behind one decode interface.
