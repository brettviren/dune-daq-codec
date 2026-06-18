# WIBEth frame format (the codec spec)

This documents the on-disk/on-wire layout of a **WIBEth** frame and its 14-bit
ADC packing — the format `dune-daq-codec` decodes for `FragmentType::kWIBEth`
(code `12`). It is reimplemented (not linked) from the read-only reference:

- `reference/fddetdataformats/include/fddetdataformats/WIBEthFrame.hpp` (+ `detail/WIBEthFrame.hxx`)
- `reference/fddetdataformats/include/fddetdataformats/Utils.hpp` (the generic bit extraction)
- `reference/detdataformats/include/detdataformats/DAQEthHeader.hpp`
- Canonical DUNE definition: EDMS document **2088713** (`https://edms.cern.ch/document/2088713`).

All multi-byte quantities are **little-endian**; the reference structs
`static_assert(std::endian::native == std::endian::little)` and rely on
trivially-copyable / standard-layout. Bit fields are numbered from the LSB.

## Fragment payload

A `kWIBEth` Fragment payload is a contiguous sequence of **fixed-size WIBEth
frames**, back to back. There is no inter-frame padding, so:

```
n_frames = payload_size_bytes / sizeof(WIBEthFrame)
```

and `payload_size_bytes % sizeof(WIBEthFrame)` MUST be 0 (an invariant — see
below). `payload_size_bytes` is the Fragment size minus the FragmentHeader.

## WIBEthFrame layout

One frame is three contiguous regions:

| region          | size (bytes) | contents                                  |
|-----------------|--------------|-------------------------------------------|
| `DAQEthHeader`  | 16           | common DAQ "eth" header (2 × 64-bit words) |
| `WIBEthHeader`  | 16           | WIB-specific header (2 × 64-bit words)     |
| `adc_words`     | 7168         | the packed ADCs: `uint64 adc_words[64][14]`|
| **total**       | **7200**     | `sizeof(WIBEthFrame)`                       |

Geometry constants (WIBEth): `s_bits_per_adc = 14`, `s_time_samples_per_frame =
64`, `s_num_channels = 64`, `word = uint64` (`s_bits_per_word = 64`), and
`s_num_adc_words_per_ts = 64 channels × 14 bits / 64 = 14` words per time sample.
Hence `adc_words[64 samples][14 words] = 896 words = 7168 bytes`.

### DAQEthHeader (16 bytes)

Word 0 — a single 64-bit little-endian word of bit fields (LSB first):

| field         | bits | notes                          |
|---------------|------|--------------------------------|
| `version`     | 6    | DAQ eth header version         |
| `det_id`      | 6    | detector id (see DetID)        |
| `crate_id`    | 10   |                                |
| `slot_id`     | 4    |                                |
| `stream_id`   | 8    |                                |
| `reserved`    | 6    | expected 0                     |
| `seq_id`      | 12   |                                |
| `block_length`| 12   |                                |

Word 1 — `timestamp` : 64-bit (the frame's starting DAQ timestamp).

`(det_id, crate_id, slot_id, stream_id)` identify the data source/geometry;
combined with the Fragment's `SourceID` they map to detector channels (the
channel-map concern is handled downstream in the bridge package, not here).

### WIBEthHeader (16 bytes)

Word 0 — 64-bit little-endian bit fields (LSB first):
`colddata_timestamp_0:15, pad_0:1, colddata_timestamp_1:15, pad_1:1, cd:1,
crc_err:2, link_valid:2, lol:1, wib_sync:1, femb_sync:2, pulser:1, calibration:1,
ready:1, context:8, version:4, channel:8`.

Word 1 — `extra_data` : 64-bit.

`version` (4 bits) is the WIB header version; `channel` (8 bits) is a frame-level
channel identifier. `pad_0`, `pad_1` are expected 0.

## ADC bit-packing (the heart of the codec)

The 64×64 ADC values are a **logical 2-D array** `[time_sample][channel]` packed
as **LSB-first 14-bit fields** into the flat `uint64` word stream, *time-sample
major*:

- For time sample `s` (0..63), its 64 channels occupy the 14-word block
  `adc_words[s][0..13]` (i.e. flat words `s*14 .. s*14+13`).
- Within that block, channel `c` (0..63) occupies the 14 bits starting at bit
  offset `14*c`, counting from the LSB of word 0 of the block, spilling into the
  next word when a field straddles a 64-bit boundary.

Extraction for `(channel c, sample s)`, matching `get_adc_2d_as_1d`:

```
block      = &adc_words[s][0]           // 14 words for this time sample
bit_lo     = 14 * c                      // absolute bit within the block
i_word     = bit_lo / 64
first_bit  = bit_lo % 64
val        = block[i_word] >> first_bit
if (64 - first_bit) < 14:                // field straddles the word boundary
    val |= block[i_word + 1] << (64 - first_bit)
adc(c, s)  = val & 0x3FFF                 // mask to 14 bits
```

This generalizes to any `(word_bits, bits_per_adc, n_channels, n_samples)` — the
basis for the data-driven descriptor in `format-descriptor-adr.md`. WIB
(`kWIB`/`kProtoWIB`) uses the same scheme with `bits_per_adc = 12` and its own
geometry; document per-detector parameters there as they are added.

## Decoded output

`dune-daq-codec` produces, per frame (or per stream of frames stitched in time):

- a dense `int16` block of shape `[n_channels][n_ticks]` (ADCs are ≤ 14 bits, so
  `int16` is exact),
- metadata: `det_id`, `crate_id`, `slot_id`, `stream_id`, `channel`, the starting
  `timestamp`, `n_ticks`, and the format version(s) seen.

Channel/tick ordering in the dense output and the DAQ→detector channel map are
the bridge package's concern (ddm-3j8.1.5), not the codec's.

## Invariants (validation — fail loudly)

For a declared WIBEth version the decoder MUST check, at minimum:

- `payload_size % 7200 == 0` (whole number of frames) and `n_frames >= 1`.
- `DAQEthHeader.version` is in the known/supported set; `WIBEthHeader.version`
  matches the expected value for that frame version.
- `reserved == 0`, `pad_0 == 0`, `pad_1 == 0` (reserved/pad bits).
- `det_id`, `crate_id`, `slot_id`, `stream_id` within their field ranges (and,
  where known, within configured detector ranges).
- every decoded ADC `< 2^14` (trivially true after masking, but assert the raw
  field width matches the descriptor).
- timestamps non-decreasing across consecutive frames of one stream (coarse
  sanity; tolerate documented gaps).

See `format-descriptor-adr.md` for how these are expressed per version and the
(stretch) trial-parse auto-detection of the *actual* layout when a declared
version is untrustworthy.
