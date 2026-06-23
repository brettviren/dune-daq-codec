#ifndef DUNE_DAQ_CODEC_CHANNELMAP_HPP
#define DUNE_DAQ_CODEC_CHANNELMAP_HPP

// Online -> offline channel mapping interface for DUNE DAQ decoding (ddm-3j8.1.13).
//
// The DUNE DAQ identifies channels by an ONLINE key — (det_id, crate_id,
// slot_id, stream_id) from the DAQEthHeader plus the local channel index within
// a frame. The offline software / Arrow `wc.frame` MUST use OFFLINE channel IDs.
// ALL channel IDs flow through this interface so the choice of map is localized.
//
// The real, file-driven map is OnlineOfflineChannelMap (OnlineOfflineChannelMap.hpp),
// which parses a DUNE ChannelMap*.txt table (ddm-3j8.1.13). IdentityChannelMap
// below remains only as a placeholder/test double. Selecting the correct table
// per run/detector is an operational concern tracked by beads ddm-3j8.1.12.

namespace dune_daq_codec {

/// Maps an online channel key to an offline channel id.
struct ChannelMap {
    virtual ~ChannelMap() = default;
    virtual int offline(unsigned det_id, unsigned crate_id, unsigned slot_id, unsigned stream_id,
                        unsigned local_channel) const = 0;
};

/// PLACEHOLDER mapping — composes a deterministic ONLINE identifier; it is NOT a
/// real offline channel id. Using it yields wc.frame traces with online IDs,
/// which is WRONG for offline use. Replace with the authoritative map
/// (ddm-3j8.1.12). Bit layout (debug-friendly, not authoritative):
/// crate(10) | slot(4) | stream(8) | local(6).
struct IdentityChannelMap : ChannelMap {
    int offline(unsigned /*det_id*/, unsigned crate_id, unsigned slot_id, unsigned stream_id,
                unsigned local_channel) const override
    {
        return static_cast<int>(((crate_id & 0x3FF) << 18) | ((slot_id & 0xF) << 14) |
                                ((stream_id & 0xFF) << 6) | (local_channel & 0x3F));
    }
};

}  // namespace dune_daq_codec

#endif  // DUNE_DAQ_CODEC_CHANNELMAP_HPP
