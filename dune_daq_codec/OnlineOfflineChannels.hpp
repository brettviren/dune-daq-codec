#ifndef DUNE_DAQ_CODEC_ONLINEOFFLINECHANNELS_HPP
#define DUNE_DAQ_CODEC_ONLINEOFFLINECHANNELS_HPP

// Online -> offline TPC channel map parsed from a DUNE `ChannelMap*.txt` table
// (ddm-3j8.1.13).
//
// These tables (DUNE's detchannelmaps / duneprototypes) do NOT share a single
// schema; two layouts exist, tied to the front-end electronics and told apart
// by column count (see docs/channel-map-schemas.md):
//
//   * 13 columns - "cold" electronics (HD, VD-bottom):
//       offlchan crate APAName wib link femb_on_link cebchan
//       plane chan_in_plane femb asic asicchan wibframechan
//     online key (crate, wib, link, wibframechan); no detid; APAName is a string.
//
//   * 12 columns - "warm" electronics (VD-prototype, VD-top):
//       offlchan detid detelement crate slot stream streamchan
//       plane chan_in_plane femb asic asicchan
//     online key (detid, crate, slot, stream, streamchan).
//
// This class auto-detects the layout and exposes the INTERSECTION of the two:
// the unified online->offline query plus the columns common to both. Per-schema
// key quirks (cold uses wib = slot+1, link = stream, wibframechan = channel;
// warm keys on detid) are handled internally.
//
// This is an initial, deliberately minimal API; the channel-argument-convention
// difference (streamchan 0-63 vs wibframechan 0-255) is documented and deferred
// to the bridge (ddm-3j8.1.5 / ddm-3j8.1.12).
//
// Pure: depends only on the standard library. Designed to be parsed ONCE and
// shared (a full far-detector map across ~150 APAs is large; do not duplicate).

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dune_daq_codec {

class OnlineOfflineChannels {
public:
    /// Which text-file layout was detected.
    enum class Schema {
        unknown,
        cold_electronics_13,  ///< HD / VD-bottom: 13 columns
        warm_electronics_12,  ///< VD-prototype / VD-top: 12 columns
    };

    /// Parse `filename` (a `ChannelMap*.txt`). Auto-detects the schema from the
    /// first data row's column count. Throws std::runtime_error if the file
    /// cannot be opened, has an unrecognized column count, or a row is malformed.
    explicit OnlineOfflineChannels(const std::string& filename);

    Schema schema() const { return schema_; }
    static const char* schema_name(Schema s);

    /// Number of channel rows loaded.
    std::size_t size() const { return rows_.size(); }

    /// Sentinel returned by offline() when no mapping exists.
    static constexpr int invalid_channel = -1;

    /// Unified online -> offline query (DUNE's
    /// get_offline_channel_from_det_crate_slot_stream_chan). The per-schema key
    /// transform is applied internally (see the class/file docs). Returns
    /// invalid_channel if the online key is not in the map.
    int offline(unsigned det, unsigned crate, unsigned slot, unsigned stream,
                unsigned channel) const;

    /// The columns common to BOTH layouts, for a given offline channel.
    struct ChannelInfo {
        unsigned offline = 0;
        unsigned crate = 0;
        unsigned plane = 0;          ///< 0:U 1:V 2:X
        unsigned chan_in_plane = 0;
        unsigned femb = 0;
        unsigned asic = 0;
        unsigned asicchan = 0;
        bool valid = false;          ///< false if the offline channel is unknown
    };

    /// Reverse lookup of the common columns by offline channel.
    /// `valid == false` if `offline` is not present.
    ChannelInfo info(unsigned offline) const;

private:
    Schema schema_ = Schema::unknown;
    std::vector<ChannelInfo> rows_;
    std::unordered_map<std::uint64_t, std::size_t> by_online_;  ///< packed key -> row
    std::unordered_map<unsigned, std::size_t> by_offline_;      ///< offline -> row
};

}  // namespace dune_daq_codec

#endif  // DUNE_DAQ_CODEC_ONLINEOFFLINECHANNELS_HPP
