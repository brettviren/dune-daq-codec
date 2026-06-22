// One-off DEV tool (NOT part of the package build): generate committable golden
// fixtures for the channel-map test, using DUNE's OWN standalone channel-map
// classes (dune::TPCChannelMapSP / dune::PD2HDChannelMapSP) as the independent
// oracle. Those classes are framework-free (plain stdlib; namespace `dune`, not
// the cetlib/ers/logging-coupled dunedaq::detchannelmaps::TPCChannelMap), so no
// DUNE stack build is needed — same situation as WIBEthFrame. See tools/README.md.
//
// For each map it pulls authoritative (online-key -> offline) pairs straight
// from DUNE's parsed table (reverse lookup by offline channel, then a forward
// lookup to confirm DUNE's own round-trip), and emits a golden the reference-free
// test_channel_map_golden checks our OnlineOfflineChannels against.
//
//   g++ -std=c++20 -I reference/detchannelmaps/src \
//     source/dune-daq-codec/tools/gen_golden_chanmap.cpp \
//     reference/detchannelmaps/src/TPCChannelMapSP.cpp \
//     reference/detchannelmaps/src/PD2HDChannelMapSP.cpp -o /tmp/genmap
//   /tmp/genmap <out_dir> <channelmaps_dir> [max_samples]
//
// Golden format (one per schema), self-describing:
//   # map <relative-path-under-channelmaps_dir>
//   # columns: det crate slot stream channel offline
//   <det> <crate> <slot> <stream> <channel> <offline>

#include "TPCChannelMapSP.h"
#include "PD2HDChannelMapSP.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// The vendored files used as the source of keys (one representative per schema).
static const char* kWarmMap = "pd2vd/PD2VDTPCChannelMap_v2.txt";  // 12-col warm
static const char* kColdMap = "pd2hd/PD2HDChannelMap_v6.txt";     // 13-col cold

// First token (offline channel = col 0) of each non-comment, non-blank line.
static std::vector<unsigned> offline_channels(const std::string& path)
{
    std::vector<unsigned> out;
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);) {
        const auto h = line.find('#');
        std::string body = (h == std::string::npos) ? line : line.substr(0, h);
        std::istringstream ls(body);
        unsigned offl;
        if (ls >> offl) out.push_back(offl);
    }
    return out;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <out_dir> <channelmaps_dir> [max_samples]\n", argv[0]);
        return 2;
    }
    const std::string out_dir = argv[1];
    const std::string maps_dir = argv[2];
    const std::size_t max_samples = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 1000;

    auto stride_for = [&](std::size_t n) { return max_samples ? std::max<std::size_t>(1, n / max_samples) : 1; };

    // -------- warm (12-col): dune::TPCChannelMapSP --------
    {
        std::string path = maps_dir + "/" + kWarmMap;
        dune::TPCChannelMapSP m;
        m.ReadMapFromFile(path);
        auto offs = offline_channels(path);
        const std::size_t stride = stride_for(offs.size());

        std::ofstream out(out_dir + "/chanmap_golden_warm.txt");
        out << "# dune-daq-codec channel-map golden; oracle: DUNE dune::TPCChannelMapSP\n";
        out << "# map " << kWarmMap << "\n";
        out << "# columns: det crate slot stream channel offline\n";
        std::size_t n = 0, written = 0;
        for (std::size_t i = 0; i < offs.size(); i += stride, ++n) {
            const unsigned offl = offs[i];
            auto ci = m.GetChanInfoFromOfflChan(offl);
            if (!ci.valid) { std::fprintf(stderr, "warm: no info for offline %u\n", offl); return 3; }
            // DUNE round-trip: the key it stored must map forward back to offl.
            auto fwd = m.GetChanInfoFromElectronicsIDs(ci.detid, ci.crate, ci.slot, ci.stream, ci.streamchan);
            if (!fwd.valid || fwd.offlchan != offl) {
                std::fprintf(stderr, "warm: DUNE round-trip mismatch at offline %u\n", offl);
                return 3;
            }
            out << ci.detid << ' ' << ci.crate << ' ' << ci.slot << ' ' << ci.stream << ' '
                << ci.streamchan << ' ' << offl << '\n';
            ++written;
        }
        std::printf("warm: wrote %zu of %zu rows (stride %zu) from %s\n", written, offs.size(), stride, kWarmMap);
    }

    // -------- cold (13-col): dune::PD2HDChannelMapSP --------
    {
        std::string path = maps_dir + "/" + kColdMap;
        dune::PD2HDChannelMapSP m;
        m.ReadMapFromFile(path);
        auto offs = offline_channels(path);
        const std::size_t stride = stride_for(offs.size());

        std::ofstream out(out_dir + "/chanmap_golden_cold.txt");
        out << "# dune-daq-codec channel-map golden; oracle: DUNE dune::PD2HDChannelMapSP\n";
        out << "# map " << kColdMap << "\n";
        out << "# columns: det crate slot stream channel offline (unified WIBEth key: stream = (link<<6)|(wibframechan/64), channel = wibframechan%64)\n";
        std::size_t n = 0, written = 0;
        for (std::size_t i = 0; i < offs.size(); i += stride, ++n) {
            const unsigned offl = offs[i];
            auto ci = m.GetChanInfoFromOfflChan(offl);
            if (!ci.valid) { std::fprintf(stderr, "cold: no info for offline %u\n", offl); return 3; }
            const unsigned slot = ci.wib - 1;  // DUNE's forward lookup re-derives wib = slot+1
            auto fwd = m.GetChanInfoFromWIBElements(ci.crate, slot, ci.link, ci.wibframechan);
            if (!fwd.valid || fwd.offlchan != offl) {
                std::fprintf(stderr, "cold: DUNE round-trip mismatch at offline %u\n", offl);
                return 3;
            }
            // Emit the key in the UNIFIED WIBEth convention (what OnlineOfflineChannels
            // and DUNE's get_offline_channel_from_det_crate_slot_stream_chan take):
            // the composite stream_id = (link<<6) | (wibframechan/64), and the
            // frame-local channel = wibframechan % 64. det is ignored for cold maps.
            const unsigned stream = (ci.link << 6) | (ci.wibframechan / 64);
            const unsigned channel = ci.wibframechan % 64;
            out << 0 << ' ' << ci.crate << ' ' << slot << ' ' << stream << ' '
                << channel << ' ' << offl << '\n';
            ++written;
        }
        std::printf("cold: wrote %zu of %zu rows (stride %zu) from %s\n", written, offs.size(), stride, kColdMap);
    }

    return 0;
}
