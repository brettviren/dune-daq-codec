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

### Open issue — the `channel` argument convention

`channel` means `streamchan` (0–63) for warm maps but `wibframechan` (0–255) for
cold maps; these are different numberings, not a rescale. The caller (the
dune-daq-wct bridge) must pass the channel index in the convention the decoded
frame uses, which is tied to the fragment's frame format (WIBEth → streamchan;
WIB2 → wibframechan). Wiring the bridge to pass the right index per format is
left to ddm-3j8.1.5 / ddm-3j8.1.12; this class only owns parse + lookup.
