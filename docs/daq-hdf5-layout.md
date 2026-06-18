# DUNE DAQ HDF5 file layout

The DUNE DAQ writes raw data as HDF5 files whose group/dataset organization is
**self-describing and versioned**. This documents that layout so the sibling
`dune-daq-hdf` package can read it with the **raw HDF5 C API** (we do not link
DUNE `hdf5libs`). Reimplemented (not linked) from the read-only reference:

- `reference/hdf5libs/include/hdf5libs/HDF5RawDataFile.hpp`
- `reference/hdf5libs/include/hdf5libs/HDF5FileLayout.hpp`
- `reference/hdf5libs/include/hdf5libs/HDF5FileLayoutParameters.hpp`
- `reference/daqdataformats/...` (TriggerRecord, TimeSlice, Fragment, SourceID)

> Note: `hdf5libs` itself uses HighFive; we deliberately use the raw HDF5 C API
> (as in `arrow-hdf`). Only the *layout conventions* below are reused.

## Records

A file holds a sequence of **records**, each a top-level HDF5 **group**:

- **TriggerRecord** files (triggered readout): group name
  `"<record_name_prefix><record_number>.<sequence_number>"`.
- **TimeSlice** files (untriggered/streaming): `record_name_prefix = "TimeSlice"`
  and no sequence component in the same sense.

The names are built from **layout parameters** (defaults shown):

| parameter                   | default        |
|-----------------------------|----------------|
| `record_name_prefix`        | `TriggerRecord`|
| `digits_for_record_number`  | `6`            |
| `digits_for_sequence_number`| `4`            |

So a record group is e.g. `TriggerRecord000123.0000`. A record is identified by
`record_id = (record_number, sequence_number)`.

## Within a record

Each record group contains a **`RawData`** group (`raw_data_group_name`; there
is also a `view_group_name` = `Views`). Under `RawData` are the datasets,
**flat**, with composite names that encode identity directly:

```
/<record group>/RawData/<SubsystemName>_0x<source_id_hex>_<FragmentTypeName>
```

Observed examples (PDVD, see "Validated" below):
- `…/RawData/TR_Builder_0x........_TriggerRecordHeader` — the **record header**
  (`record_header_dataset_name = "TriggerRecordHeader"`), holding a serialized
  `TriggerRecordHeader` (trigger/run numbers, timestamp, component requests).
- `…/RawData/Detector_Readout_0x00000190_WIBEth` — a detector **Fragment**
  dataset (`FragmentHeader` + payload), `SourceID = (kDetectorReadout, 0x190)`,
  `FragmentType = kWIBEth`.
- likewise `…_TDEEth`, `…_Trigger_Primitive`, `…_Hardware_Signal`, etc.

> The `path_param_list` in `filelayout_params` (with `detector_group_name`,
> `element_name_prefix`, per-subsystem digit widths — e.g. TPC declares prefix
> `Link`, others `Element`) describes a *possible* nested scheme, but the
> **actual datasets observed are flat composite-named under `RawData`**. This is
> exactly why the **traverse + classify by name** reading strategy (below) is
> preferred over constructing paths from the layout parameters.

`SourceID` = `{ subsystem, id }` with
`Subsystem ∈ {kUnknown=0, kDetectorReadout=1, kHwSignalsInterface=2,
kTrigger=3, kTRBuilder=4}`. The composite name's `SubsystemName` /
`FragmentTypeName` are the enum names (`Detector_Readout`, `TR_Builder`,
`Trigger`, `HW_Signals_Interface`; `WIBEth`, `TDEEth`, `Trigger_Primitive`, …).
The file also stores `source_id_path_map`, `fragment_type_source_id_map`,
`subdetector_source_id_map`, and `record_header_source_id` as attributes, so a
reader may resolve identity from attributes instead of parsing names.

## Self-description & versioning (important)

The file embeds its `HDF5FileLayout` configuration (the parameters above) plus a
**layout version** (with min/max-allowed checks in the reference). It may also
store a `SourceID → path` map and per-object attributes (record type, etc.).
**Do not hard-code paths**: record-name prefixes and digit widths vary by
version/config.

## Fragment (the dataset payload)

Each fragment dataset is a flat byte blob = `FragmentHeader` followed by payload.
From `daqdataformats`:

- `FragmentHeader` begins with `fragment_header_marker = 0x11112222` (uint32 @0),
  then `version` (uint32 @4), then `size` (uint64 @8). `sizeof(FragmentHeader)` is
  **72 bytes** (validated below).
- `FragmentHeader.size` = total bytes; payload size = `size - sizeof(FragmentHeader)`.
- `FragmentHeader.fragment_type` (`FragmentType`) selects the payload codec; e.g.
  `kWIBEth = 12` (see `wibeth-format.md`), `kWIB = 2`, `kDAPHNE = 3`, … (full list
  in `reference/daqdataformats/include/daqdataformats/FragmentHeader.hpp`).
- Other header fields: trigger/run numbers, window begin/end timestamps,
  `SourceID`, `DetID` element, error bits.

`dune-daq-hdf` yields, per record, the set of `(record_id, SourceID, fragment
bytes, fragment_type)`; `dune-daq-codec` decodes the bytes per `fragment_type`.

## Recommended reading strategy (raw HDF5, no hdf5libs)

Two viable approaches; recommended is the second (most robust to layout drift):

1. **Layout-driven**: read the embedded layout parameters + version, then
   construct the exact dataset paths.
2. **Traverse + classify** (recommended): enumerate all datasets via `H5Ovisit`
   (as `arrow-hdf::scan` does), then classify each by path pattern / attributes
   into record-header vs. fragment, recovering `record_id` and `SourceID` from
   the path and/or attributes. This needs no path construction and tolerates
   layout variation; cross-check against the embedded version where present.

Version handling and invariant checks for the *reader* parallel those for the
codec (see `format-descriptor-adr.md`): treat the layout version as advisory,
verify structural invariants (record-header present, fragment header `size`
consistent with the dataset byte length, `SourceID`/`FragmentType` in range), and
fail loudly on violation.

## Validated against a real PDVD file

Cross-checked against a ProtoDUNE Vertical Drift (PDVD) raw file
(`np02vd_raw_run040380_0019_…`, ~4 GB), 2025-11-05. Concrete observations:

- **Records** are top-level groups `TriggerRecord<rec>.<seq>`. The root
  `filelayout_params` attribute (JSON) declared `digits_for_record_number = 5`
  (not the library default 6) and `digits_for_sequence_number = 4`, e.g.
  `TriggerRecord01663.0000` — confirming versions/digit-widths must be read, not
  hard-coded. A `filelayout_version` attribute is also present.
- **Layout is flat** under `RawData`: `…/RawData/<Subsystem>_0x<sid>_<FragType>`.
  One record contained, e.g., 96 `Detector_Readout_…_WIBEth`, 96
  `…_TDEEth`, 12 `Trigger_…_Trigger_Primitive`, plus `Trigger_Activity`,
  `Trigger_Candidate`, `HW_Signals_Interface_…_Hardware_Signal`, and one
  `TR_Builder_…_TriggerRecordHeader`. So a single file mixes multiple detector
  formats (here both **WIBEth** and **TDEEth**).
- **FragmentHeader validated** on `Detector_Readout_0x00000190_WIBEth`
  (dataset = 1,195,272 bytes, an `[N][1]` uint8 dataset): bytes[0:4] =
  `0x11112222` (marker ✓); bytes[8:16] = `1195272` = the dataset size (the
  `size` field ✓). With `sizeof(FragmentHeader) = 72`, payload = `1,195,200 =
  166 × 7200`, i.e. **166 WIBEth frames @ 7200 bytes** — confirming
  `wibeth-format.md`.
- **Version drift, observed in the wild:** the FragmentHeader `version` field
  read **5**, while the current reference defines `s_fragment_header_version = 6`.
  This is a concrete example of the silent-drift problem the version-robustness
  strategy must handle (treat declared versions as advisory; rely on structural
  invariants). See `format-descriptor-adr.md`.

### Candidate test fixture

A small, committable fixture (the 4 GB file cannot be committed) for the codec
tests: extract one WIBEth Fragment (or a single 7200-byte WIBEth frame) from
`/TriggerRecord01663.0000/RawData/Detector_Readout_0x00000190_WIBEth` and pin it
with expected ADC values. The expected ADCs must come from an INDEPENDENT source
of truth (e.g. a one-off run of DUNE's own tools outside our build, or DUNE
python), since validating our reimplemented decoder against its own output proves
nothing.

## FragmentHeader.size is unreliable — use the dataset byte length

Reliable cross-checks (via our reader) across PDHD+PDVD show the in-payload
`FragmentHeader.size` field does **not** dependably equal the fragment's true
byte length:

| file (first WIBEth frag) | dataset bytes (authoritative) | header.size field |
|--------------------------|-------------------------------|-------------------|
| np02vd run040380         | 1,195,272 (= 72 + 166×7200)   | 1,195,272 (equal) |
| np02vd run039252         | 1,108,872 (= 72 + 154×7200)   | 1,048,576 (1 MiB; **< actual**) |
| np02vd run039349         | 712,872   (= 72 + 99×7200)    | 655,360 (640 KiB) |
| np04hd run027980         | 669,672   (= 72 + 93×7200)    | 669,440           |

The **HDF5 dataset byte length is authoritative** and is consistently
frame-aligned (`72 + N × frame_bytes`); `header.size` is sometimes a round buffer
size and sometimes **smaller than the actual content** (so trusting it would
*truncate* frames). Therefore:

- size a fragment's payload from the **dataset length**, not `FragmentHeader.size`;
- the frame-count invariant is `(dataset_bytes − 72) % frame_bytes == 0`, **not**
  `header.size == dataset_bytes`;
- validity is checked via the marker (`0x11112222`), not the size field.

This is a concrete instance of the silent-format-drift hazard and is reflected in
the reader (`FragmentView::size` is the dataset length) and must be reflected in
the codec's invariants (ddm-3j8.1.7.3).
