# Vendored DUNE channel-map tables

These `*.txt` tables are **copied verbatim** from the DUNE-DAQ `detchannelmaps`
package and committed here so this package has no build- or run-time dependency
on `detchannelmaps` (which would pull in unwanted code). They are read by
`dune_daq_codec::OnlineOfflineChannels`.

## Source

- Repository: https://github.com/DUNE-DAQ/detchannelmaps
- Commit: `bf467ea4523fb8a07a6c3cc2af0c036014df49c9` (branch `develop`, 2026-04-02)
- Upstream path: `config/pd2hd/` and `config/pd2vd/`

No `LICENSE`/`COPYING` file is present in the upstream repository at that commit.
These are DUNE collaboration data tables vendored for interoperability; update
them from upstream as the collaboration revises the maps, keeping this file's
commit reference in sync.

## What is vendored (and what is not)

Only the **TPC** channel maps for the detectors this project currently targets
(ProtoDUNE-VD = "pd2vd", ProtoDUNE-HD = "pd2hd"), and only the two layouts that
`OnlineOfflineChannels` parses:

| file pattern | columns | electronics | detector |
|---|---|---|---|
| `pd2hd/PD2HDChannelMap_v*.txt` | 13 | cold | ProtoDUNE-HD |
| `pd2vd/PD2VDBottomTPCChannelMap_v*.txt` | 13 | cold | ProtoDUNE-VD bottom |
| `pd2vd/PD2VDTPCChannelMap_v*.txt` | 12 | warm | ProtoDUNE-VD |
| `pd2vd/PD2VDTopTPCChannelMap_v*.txt` | 12 | warm | ProtoDUNE-VD top |

NOT vendored (out of current scope): other test stands (`50L`, `iceberg`,
`hdcoldbox`, `protodunesp1`), the 9-column legacy `vdcoldbox`/HardwareMap tables
(an older format `OnlineOfflineChannels` does not parse), and `PD2HDHardwareMap.txt`.
See `../docs/channel-map-schemas.md` for the column meanings and the 12-/13-column
schema differences.

## Choosing a version

Multiple `_v1.._vN` versions are kept because the correct map depends on the run
(detector configuration at data-taking time); there is no run→version table.
The correct file is selected by an expert via job configuration — see the
Phlex-level beads issues for delivering the chosen path through a resource node.
