#include "dune_daq_codec/OnlineOfflineChannels.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace dune_daq_codec {

namespace {

// Pack a 5-field online key into one 64-bit value. Field widths (det:8, crate:16,
// slot:8, stream:16, channel:16) comfortably cover real ranges without overlap.
std::uint64_t pack_key(unsigned det, unsigned crate, unsigned slot, unsigned stream,
                       unsigned channel)
{
    return (static_cast<std::uint64_t>(det & 0xFFu) << 56) |
           (static_cast<std::uint64_t>(crate & 0xFFFFu) << 40) |
           (static_cast<std::uint64_t>(slot & 0xFFu) << 32) |
           (static_cast<std::uint64_t>(stream & 0xFFFFu) << 16) |
           static_cast<std::uint64_t>(channel & 0xFFFFu);
}

unsigned to_uint(const std::string& tok, const std::string& field, std::size_t line_no)
{
    try {
        return static_cast<unsigned>(std::stoul(tok));
    }
    catch (const std::exception&) {
        std::ostringstream os;
        os << "OnlineOfflineChannels: line " << line_no << ": column '" << field
           << "' is not an unsigned integer: '" << tok << "'";
        throw std::runtime_error(os.str());
    }
}

}  // namespace

const char* OnlineOfflineChannels::schema_name(Schema s)
{
    switch (s) {
    case Schema::cold_electronics_13: return "cold_electronics_13";
    case Schema::warm_electronics_12: return "warm_electronics_12";
    default: return "unknown";
    }
}

OnlineOfflineChannels::OnlineOfflineChannels(const std::string& filename)
{
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("OnlineOfflineChannels: cannot open file: " + filename);
    }

    std::string line;
    std::size_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;

        // Strip a trailing comment, then split on whitespace.
        const auto hash = line.find('#');
        std::string body = (hash == std::string::npos) ? line : line.substr(0, hash);

        std::istringstream ls(body);
        std::vector<std::string> tok;
        for (std::string t; ls >> t;) tok.push_back(t);
        if (tok.empty()) continue;  // blank or comment-only line

        // Lock the schema on the first data row.
        if (schema_ == Schema::unknown) {
            if (tok.size() == 13) schema_ = Schema::cold_electronics_13;
            else if (tok.size() == 12) schema_ = Schema::warm_electronics_12;
            else {
                std::ostringstream os;
                os << "OnlineOfflineChannels: line " << line_no << ": expected 12 or 13 "
                   << "columns, got " << tok.size();
                throw std::runtime_error(os.str());
            }
        }

        const std::size_t want = (schema_ == Schema::cold_electronics_13) ? 13 : 12;
        if (tok.size() != want) {
            std::ostringstream os;
            os << "OnlineOfflineChannels: line " << line_no << ": expected " << want
               << " columns (" << schema_name(schema_) << "), got " << tok.size();
            throw std::runtime_error(os.str());
        }

        ChannelInfo ci;
        ci.valid = true;
        std::uint64_t key = 0;

        if (schema_ == Schema::cold_electronics_13) {
            // offlchan crate APAName wib link femb_on_link cebchan
            // plane chan_in_plane femb asic asicchan wibframechan
            ci.offline = to_uint(tok[0], "offlchan", line_no);
            ci.crate = to_uint(tok[1], "crate", line_no);
            // tok[2] = APAName (string), tok[5]=femb_on_link, tok[6]=cebchan: skipped
            const unsigned wib = to_uint(tok[3], "wib", line_no);
            const unsigned link = to_uint(tok[4], "link", line_no);
            ci.plane = to_uint(tok[7], "plane", line_no);
            ci.chan_in_plane = to_uint(tok[8], "chan_in_plane", line_no);
            ci.femb = to_uint(tok[9], "femb", line_no);
            ci.asic = to_uint(tok[10], "asic", line_no);
            ci.asicchan = to_uint(tok[11], "asicchan", line_no);
            const unsigned wibframechan = to_uint(tok[12], "wibframechan", line_no);
            // No detid in a cold map; det is ignored in the online key.
            key = pack_key(0, ci.crate, wib, link, wibframechan);
        }
        else {  // warm_electronics_12
            // offlchan detid detelement crate slot stream streamchan
            // plane chan_in_plane femb asic asicchan
            ci.offline = to_uint(tok[0], "offlchan", line_no);
            const unsigned detid = to_uint(tok[1], "detid", line_no);
            // tok[2] = detelement: skipped
            ci.crate = to_uint(tok[3], "crate", line_no);
            const unsigned slot = to_uint(tok[4], "slot", line_no);
            const unsigned stream = to_uint(tok[5], "stream", line_no);
            const unsigned streamchan = to_uint(tok[6], "streamchan", line_no);
            ci.plane = to_uint(tok[7], "plane", line_no);
            ci.chan_in_plane = to_uint(tok[8], "chan_in_plane", line_no);
            ci.femb = to_uint(tok[9], "femb", line_no);
            ci.asic = to_uint(tok[10], "asic", line_no);
            ci.asicchan = to_uint(tok[11], "asicchan", line_no);
            key = pack_key(detid, ci.crate, slot, stream, streamchan);
        }

        const std::size_t row = rows_.size();
        rows_.push_back(ci);
        by_online_.emplace(key, row);
        by_offline_.emplace(ci.offline, row);
    }

    if (schema_ == Schema::unknown) {
        throw std::runtime_error("OnlineOfflineChannels: no data rows in file: " + filename);
    }
}

int OnlineOfflineChannels::offline(unsigned det, unsigned crate, unsigned slot,
                                   unsigned stream, unsigned channel) const
{
    std::uint64_t key = 0;
    if (schema_ == Schema::cold_electronics_13) {
        // Unified online->offline for cold electronics (HD / VD-bottom), matching
        // DUNE's PD2HDChannelMapSPPluginBase::get_offline_channel_from_det_crate_
        // slot_stream_chan. det is ignored (the table is one detector); slot maps
        // to wib = slot+1. The DAQEthHeader stream_id is COMPOSITE: bit 6 is the
        // cold-electronics link (0/1) and the low 2 bits select a 64-channel
        // sub-block within the WIB's 256-channel wibframechan space, so
        //   wibframechan = 64*(stream & 0x3) + channel.
        // (channel is the WIBEth frame-local index 0..63.) Streams other than
        // 0..3 / 64..67 are invalid.
        if (stream & 0xbcu) return invalid_channel;
        const unsigned link = (stream >> 6) & 0x1u;
        const unsigned wibframechan = 64u * (stream & 0x3u) + channel;
        key = pack_key(0, crate, slot + 1, link, wibframechan);
    }
    else {
        // Warm electronics (VD / TDE): stream and channel are used directly as
        // the (stream, streamchan) key columns (matches dune::TPCChannelMapSP).
        key = pack_key(det, crate, slot, stream, channel);
    }
    const auto it = by_online_.find(key);
    if (it == by_online_.end()) return invalid_channel;
    return static_cast<int>(rows_[it->second].offline);
}

OnlineOfflineChannels::ChannelInfo OnlineOfflineChannels::info(unsigned offline) const
{
    const auto it = by_offline_.find(offline);
    if (it == by_offline_.end()) return ChannelInfo{};  // valid == false
    return rows_[it->second];
}

}  // namespace dune_daq_codec
