# Format coverage (observed across PDHD + PDVD)

Inventory of the data parts we must support, surveyed across 10 real raw files
(4 PDHD `np04hd` runs 2024; 6 PDVD `np02vd` runs 2025). All files share the same
container layout (see `daq-hdf5-layout.md`): `TriggerRecord<5>.<4>` groups, a
`RawData` group, flat composite-named datasets
`<Subsystem>_0x<sourceid>_<FragmentType>`. **FragmentHeader version = 5** in every
sampled file (the reference HEAD defines `6` — we target **v5**, treating the
version as advisory per the version-robustness strategy).

## Detector readout (ADC) — the codec's job

| FragmentType    | code | detector(s)            | role / electronics         | frame | notes |
|-----------------|------|------------------------|----------------------------|-------|-------|
| `WIBEth`        | 12   | PDHD (all TPC); PDVD **bottom** | TPC                | **7200 B** | validated: payloads = N×7200 in both detectors (e.g. 93×, 99×, 166×) |
| `TDEEth`        | 15   | PDVD **top**           | TPC                        | ~7200 B (likely) | payload = 101×7200 exact → same geometry probable; confirm vs reference `TDEEthFrame` |
| `DAPHNE`        | 3    | PDHD + PDVD            | PDS (self-triggered)       | TBD   | small/variable; **not** WIB-shaped — separate decode |
| `DAPHNEStream`  | 13   | PDHD + PDVD            | PDS (streaming)            | TBD   | larger; separate decode |
| `CRT`           | 14   | some PDHD runs         | Cosmic Ray Tagger          | TBD   | detector readout; separate decode |

Key points from the survey:

- **TPC**: PDHD has one electronics type → `WIBEth`. PDVD has two (`WIBEth`
  "bottom", which matches PDHD, and `TDEEth` "top"). `WIBEth` and `TDEEth` look
  like the same regular LSB-first bit-array packing (both ×7200) → one
  data-driven descriptor likely covers both (and `CRT`, pending check).
- **PDS** (`DAPHNE`, `DAPHNEStream`): present in **both** detectors, small data
  volume, but a **different structure** from the TPC bit-array. `DAPHNE`
  (self-triggered) is the most likely candidate for an *irregular / variable*
  layout — i.e. the case the descriptor ADR flagged as a possible reason to
  reconsider DIY vs. a grammar for that specific format. To be evaluated against
  the reference `DAPHNEFrame` / `DAPHNEStreamFrame` when PDS decode is taken up.

## Non-ADC parts (surfaced by the reader; decode scope TBD)

| dataset                              | nature                                   |
|--------------------------------------|------------------------------------------|
| `TR_Builder_…_TriggerRecordHeader`   | the record header (trigger/run/timestamp)|
| `Trigger_…_Trigger_Primitive/Activity/Candidate` | trigger objects (`trgdataformats`) |
| `HW_Signals_Interface_…_Hardware_Signal` | HSI signals                          |

`dune-daq-hdf` surfaces all of these as `(record_id, SourceID, fragment bytes,
fragment_type)`; which non-ADC parts we decode (vs. carry as opaque metadata) is
a scope decision for the bridge/Phlex-source layers.

## Implication for the codec

The codec is a small **family** keyed by `FragmentType`, each entry a descriptor
+ invariants:

1. **TPC bit-array** (`WIBEth`, `TDEEth`, likely `CRT`): the regular
   parameterized LSB-first packing — one generic decode loop, one descriptor per
   format/version. **Start here (WIBEth).**
2. **PDS** (`DAPHNE`, `DAPHNEStream`): separate decode; evaluate
   regular-vs-irregular per format.
