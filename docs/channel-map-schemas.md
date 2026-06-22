# DUNE channel-map text-file schemas (ddm-3j8.1.13)

The DAQ identifies a TPC channel by an *online* key; the offline software and the
Arrow `wc.frame` must use the *offline* channel id. The authoritative mapping is
distributed as whitespace-delimited `ChannelMap*.txt` tables (see DUNE's
`detchannelmaps` and `duneprototypes`). `OnlineOfflineChannels` parses such a
file and answers the online→offline query.

These tables do **not** share one schema. Two layouts exist, distinguished by
column count, tied to the front-end electronics:

| electronics | detectors | columns |
|---|---|---|
| **cold** (BDE/WIB, `WIB2Frame`) | HD, VD-bottom | **13** |
| **warm** (TDE / WIBEth `Eth`) | VD-prototype, VD-top | **12** |

### Out of scope

A third, **9-column** legacy format exists in `detchannelmaps/config` — the
`vdcoldbox` `vdcbce_chanmap_v*` tables and `PD2HDHardwareMap.txt` (a hardware,
not channel, map). These are not the modern `ChannelMap*.txt` TPC tables and are
**not** supported; `OnlineOfflineChannels` throws on an unrecognized column
count rather than guessing.

## The two layouts

The difference is **not** "12 columns + 1 extra". Only a subset of columns is
common; each layout has columns the other lacks, and the online key is built
from different columns.

### 13-column — cold electronics (`PD2HDChannelMapSP`)

```
offlchan crate APAName wib link femb_on_link cebchan plane chan_in_plane femb asic asicchan wibframechan
```

- Online key: **(crate, wib, link, wibframechan)**. No `detid` column — a cold
  map is implicitly a single detector.
- `APAName` (col 2) is a **string** (e.g. `APA_P02SU`) — a reliable secondary
  signal that a row is the 13-column layout.
- `wibframechan` ranges 0–255 (the whole WIB frame, both FEMBs).

### 12-column — warm electronics (`TPCChannelMapSP`)

```
offlchan detid detelement crate slot stream streamchan plane chan_in_plane femb asic asicchan
```

- Online key: **(detid, crate, slot, stream, streamchan)**. `detid` *is* part of
  the key here.
- `streamchan` ranges 0–63 (channels within one stream).
- `detelement` = CRP number (VD) or APA number (HD); `slot` = WIB (BDE/HD) or
  card (TDE); `stream` = Hermes stream (BDE/HD) or 0 (TDE).

## Columns in common (the API intersection)

`offlchan`, `crate`, `plane`, `chan_in_plane`, `femb`, `asic`, `asicchan`.

## Reconciling the two online keys

Both reduce to DUNE's unified query
`get_offline_channel_from_det_crate_slot_stream_chan(det, crate, slot, stream, channel)`:

| query arg | warm (12) | cold (13) |
|---|---|---|
| `det`     | `detid` (used) | ignored |
| `crate`   | `crate` | `crate` |
| `slot`    | `slot`  | **`wib = slot + 1`** |
| `stream`  | `stream`| `link` |
| `channel` | `streamchan` (0–63) | `wibframechan` (0–255) |

`OnlineOfflineChannels::offline(det, crate, slot, stream, channel)` applies the
per-schema transform internally.

### The `(stream, channel)` convention (resolved)

The caller always passes the raw DAQEthHeader `stream_id` and the WIBEth
frame-local channel index (`get_adc` index, 0–63). `OnlineOfflineChannels`
applies the per-schema transform internally, mirroring DUNE's unified
`get_offline_channel_from_det_crate_slot_stream_chan`:

- **warm (12-col):** `stream` and `channel` are the map key columns `stream` and
  `streamchan` directly.
- **cold (13-col):** the `stream_id` is COMPOSITE — bit 6 is the cold-electronics
  `link` (0/1) and the low 2 bits select a 64-channel sub-block of the WIB's
  256-channel `wibframechan` space:

  ```
  link         = (stream >> 6) & 1
  wibframechan = 64 * (stream & 0x3) + channel        # channel = WIBEth chan 0..63
  ```

  Stream ids outside `{0,1,2,3, 64,65,66,67}` are invalid (return -1). This is
  the exact decomposition in DUNE's `PD2HDChannelMapSPPluginBase`. Verified end
  to end on real PDHD data: all 10240 channels of a 4-APA record map to the
  10240 distinct offline ids 0..10239 (ddm-3j8.1.16).
