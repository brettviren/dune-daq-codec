#ifndef DUNE_DAQ_CODEC_ONLINEOFFLINECHANNELMAP_HPP
#define DUNE_DAQ_CODEC_ONLINEOFFLINECHANNELMAP_HPP

// A concrete ChannelMap backed by a parsed DUNE ChannelMap*.txt table
// (ddm-3j8.1.13 / ddm-3j8.1.12).
//
// This replaces IdentityChannelMap with the real online->offline mapping. It is
// a thin adapter: dune_daq_codec::OnlineOfflineChannels does the parsing,
// schema auto-detection (12-col warm / 13-col cold) and the lookup; this class
// just exposes it through the bridge's ChannelMap interface so ToFrame's
// traces_from() emits OFFLINE channel ids.
//
// Parse ONCE and share: construct a single instance and pass it (by reference,
// as ChannelMap&) to every decoding/bridging call. A full far-detector map is
// large; do not build one per consumer (ddm-3j8.1.13).

#include "dune_daq_codec/ChannelMap.hpp"
#include "dune_daq_codec/OnlineOfflineChannels.hpp"

#include <string>

namespace dune_daq_codec {

/// ChannelMap backed by a DUNE ChannelMap*.txt file.
class OnlineOfflineChannelMap : public ChannelMap {
public:
    /// Parse `filename` (throws std::runtime_error on open/parse failure; see
    /// dune_daq_codec::OnlineOfflineChannels).
    explicit OnlineOfflineChannelMap(const std::string& filename) : map_(filename) {}

    /// Online (det, crate, slot, stream, local_channel) -> offline channel id.
    /// Returns dune_daq_codec::OnlineOfflineChannels::invalid_channel (-1) when
    /// the online key is not in the map.
    int offline(unsigned det_id, unsigned crate_id, unsigned slot_id, unsigned stream_id,
                unsigned local_channel) const override
    {
        return map_.offline(det_id, crate_id, slot_id, stream_id, local_channel);
    }

    /// Access the underlying parsed map (schema, size, info() reverse lookup).
    const dune_daq_codec::OnlineOfflineChannels& channels() const { return map_; }

private:
    dune_daq_codec::OnlineOfflineChannels map_;
};

}  // namespace dune_daq_codec

#endif  // DUNE_DAQ_CODEC_ONLINEOFFLINECHANNELMAP_HPP
