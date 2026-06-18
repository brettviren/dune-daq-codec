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

Each record group contains:

1. A **record-header dataset** holding a serialized `TriggerRecordHeader`
   (or `TimeSliceHeader`): trigger/run numbers, timestamp, the component
   requests, etc.
2. A **detector-group hierarchy** of **Fragment datasets**, addressed by
   `SourceID`. The path components come from the layout parameters:

   | parameter                  | default     |
   |----------------------------|-------------|
   | `detector_group_type`      | (config)    |
   | `detector_group_name`      | (config)    |
   | `element_name_prefix`      | `Element`   |
   | `digits_for_element_number`| `5`         |
   | `path_params_list`         | per-`Subsystem` group/region/element naming |

   A fragment dataset path looks like (schematically):
   `<record group>/<detector_group_name>/<subsystem path params…>/<element_name_prefix><element_number>`
   e.g. `…/Element00005`. The leaf **dataset is the Fragment bytes**
   (`FragmentHeader` + payload).

`SourceID` = `{ subsystem, id }` with
`Subsystem ∈ {kUnknown=0, kDetectorReadout=1, kHwSignalsInterface=2,
kTrigger=3, kTRBuilder=4}`. Detector data lives under `kDetectorReadout`.

## Self-description & versioning (important)

The file embeds its `HDF5FileLayout` configuration (the parameters above) plus a
**layout version** (with min/max-allowed checks in the reference). It may also
store a `SourceID → path` map and per-object attributes (record type, etc.).
**Do not hard-code paths**: record-name prefixes and digit widths vary by
version/config.

## Fragment (the dataset payload)

Each fragment dataset is a flat byte blob = `FragmentHeader` followed by payload.
From `daqdataformats`:

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
