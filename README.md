# dune-daq-codec

A **pure, dependency-free** decoder (and, later, encoder) for DUNE DAQ detector
data: it converts the *packed, unaligned* ADC sample formats found in DUNE DAQ
fragments into a **dense 2-D array** (int16, channels × time-ticks) plus
metadata, in common types (`std::vector`, `std::byte`) that downstream code can
hand to Arrow / Wire-Cell.

It does **not** read HDF5 (that is `dune-daq-hdf`) and it does **not** depend on
the DUNE DAQ software. We reimplement the formats from their documented layouts;
the DUNE clones under `reference/` (`daqdataformats`, `detdataformats`,
`fddetdataformats`, `hdf5libs`) are **read-only reference material only** — this
package links none of them, not even for unit tests (project rule).

## Scope (initial)

- **Decode** only (encode later).
- **WIBEth** first (`FragmentType::kWIBEth = 12`, 14-bit ADC), then other
  detectors (WIB 12-bit, …) incrementally.
- A **data-driven format descriptor** parameterizes one generic bit-unpack loop,
  so new format versions can be added as *data* rather than new code paths
  (see `docs/format-descriptor-adr.md`).

## Documentation (the spec we reimplement against)

These are the reference/spec products (beads ddm-3j8.1.2), derived by reading the
`reference/` clones and the cited DUNE documents:

- [`docs/wibeth-format.md`](docs/wibeth-format.md) — the WIBEth frame layout and
  the 14-bit ADC bit-packing (the core codec spec).
- [`docs/daq-hdf5-layout.md`](docs/daq-hdf5-layout.md) — the DUNE DAQ HDF5 file
  organization (primarily for the sibling `dune-daq-hdf` package).
- [`docs/format-descriptor-adr.md`](docs/format-descriptor-adr.md) — ADR for the
  DIY data-driven descriptor (vs. a formal grammar such as Kaitai Struct), the
  descriptor design, and the version-robustness strategy.

## Status

Documentation first (ddm-3j8.1.2). Implementation (ddm-3j8.1.1) follows, gated on
the project-owned DAQ types (ddm-3j8.1.3) and the version-robustness design
(ddm-3j8.1.7.3).
