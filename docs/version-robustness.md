# Version robustness & format invariants

How `dune-daq-codec` defends against silent DAQ format drift (ddm-3j8.1.7.3).
DAQ formats change — sometimes without bumping the embedded version fields — so
the decoder must **detect violations of per-format invariants and fail loudly**,
and (stretch) **auto-detect** the actual layout when the declared version can't
be trusted. Realized via the data-driven `FormatDescriptor` (ADR-001).

## Where "version" comes from — and why it's only advisory

Candidate version signals: `FragmentHeader.version` (observed **5** in all
2024–2025 PDHD/PDVD files; reference HEAD = 6), `DAQEthHeader.version`,
`<frame>Header.version`, and `FragmentType`. We treat all of these as
**advisory**: the user's premise (and observed reality) is that they may not
change when the layout silently does. The descriptor is selected primarily by
`FragmentType` (which the HDF5 dataset name carries reliably), and the
structural invariants below — not the version fields — decide acceptance.

## The descriptor

`FormatDescriptor` (`dune_daq_codec/FormatDescriptor.hpp`) is plain data, one per
(detector format, generation):

```
fragment_type, name, word_bits, bits_per_adc, n_channels, n_samples,
frame_header_bytes, frame_bytes
```

`self_consistent()` checks the geometry adds up (`n_channels*bits_per_adc` is a
whole number of words; `frame_bytes == frame_header_bytes + adc_bytes_per_frame`).
Adding a new format/generation is **adding a descriptor row (data)**, not new
code — the rebuild-free goal of ADR-001. Registry: `descriptors[]` +
`descriptor_for(FragmentType)`.

## The invariants (validate / require_valid)

`validate(descriptor, total_bytes, header)` where **`total_bytes` is the
authoritative HDF5 dataset byte length** (see below). It checks:

1. **descriptor self-consistency** (geometry adds up);
2. **marker** — `FragmentHeader.fragment_header_marker == 0x11112222` (this, not
   the version, is the validity gate);
3. **frame alignment** — `(total_bytes − sizeof(FragmentHeader)) % frame_bytes
   == 0`; the quotient is the frame count;
4. declared `fragment_type` vs the descriptor — **advisory only** (recorded, not
   a failure; the dataset name already selected the descriptor).

`require_valid(...)` throws `std::runtime_error` (joined problems) on any
failure — fail loud.

### Critical: trust the dataset length, not `FragmentHeader.size`

Reliably observed across PDHD+PDVD (see `daq-hdf5-layout.md`), the in-payload
`FragmentHeader.size` field is **unreliable** — sometimes a round buffer size
(1 MiB, 640 KiB), and sometimes **smaller than the actual stored content** (so
trusting it would truncate frames). The HDF5 **dataset byte length** is
authoritative and consistently frame-aligned. Hence the frame-count invariant
uses `total_bytes` (the dataset length, from `dune_daq_hdf::FragmentView::size`),
**never** `header.size`. This is the single most important robustness decision.

## Auto-detection (stretch)

`auto_detect(total_bytes, header)` runs every registered descriptor's invariants
and returns the **unique** one that passes (nullptr if none or several). It is a
fallback for an untrustworthy `fragment_type`. Note its inherent limit: formats
that share `frame_bytes` (WIBEth and TDEEth both 7200 B) are **indistinguishable
by structure alone** — the dataset-name suffix must disambiguate them. So
auto-detect complements, but does not replace, name-based selection.

## Future strengthening (not yet implemented)

Additional per-descriptor invariants the decode path can add when frames are
parsed (ddm-3j8.1.1): reserved/pad bits zero (`DAQEthHeader.reserved`,
WIBEth `pad_0/pad_1`), `det_id`/`crate`/`slot`/`stream` within configured
ranges, ADC field width matches, timestamps non-decreasing across a stream.
These are per-frame and best checked during decoding; the descriptor can carry a
list of such checks as it grows.
