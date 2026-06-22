// Golden validation of OnlineOfflineChannels against DUNE's OWN channel-map
// classes (ddm-3j8.1.16). The committed golden fixtures (chanmap_golden_*.txt,
// produced by tools/gen_golden_chanmap.cpp from dune::TPCChannelMapSP /
// dune::PD2HDChannelMapSP) carry authoritative (online-key -> offline) pairs.
// This test loads the vendored map, asks OnlineOfflineChannels for each key, and
// asserts it matches DUNE's offline channel. Depends ONLY on committed fixtures +
// vendored maps + our codec — NOT on reference/ (project rule).

#include "dune_daq_codec/OnlineOfflineChannels.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR must be defined"
#endif
#ifndef CHANNELMAPS_DIR
#error "CHANNELMAPS_DIR must be defined"
#endif

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++fails;
}

// Validate one golden file: read its "# map <rel>" header, load that vendored
// map, and confirm every key resolves to the golden (DUNE) offline channel.
static void validate(const std::string& golden_path)
{
    std::ifstream in(golden_path);
    if (!in) { std::cerr << "cannot open " << golden_path << "\n"; std::exit(2); }

    std::string map_rel;
    std::string line;
    long checked = 0, mism = 0;
    std::unique_ptr<dune_daq_codec::OnlineOfflineChannels> map;

    while (std::getline(in, line)) {
        if (line.rfind("# map ", 0) == 0) {
            map_rel = line.substr(6);
            map = std::make_unique<dune_daq_codec::OnlineOfflineChannels>(
                std::string(CHANNELMAPS_DIR) + "/" + map_rel);
            continue;
        }
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ls(line);
        unsigned det, crate, slot, stream, channel;
        int offline;
        if (!(ls >> det >> crate >> slot >> stream >> channel >> offline)) continue;
        if (!map) { std::cerr << golden_path << ": data before '# map' header\n"; std::exit(2); }

        ++checked;
        if (map->offline(det, crate, slot, stream, channel) != offline) ++mism;
    }

    check(map != nullptr && checked > 0, golden_path + ": loaded map + keys (" +
                                             std::to_string(checked) + " keys, map " + map_rel + ")");
    check(mism == 0, golden_path + ": every key matches the DUNE golden offline");
    if (mism) std::cerr << "  " << mism << " / " << checked << " mismatches\n";
}

int main()
{
    const std::string dir = FIXTURES_DIR;
    validate(dir + "/chanmap_golden_warm.txt");  // 12-col, dune::TPCChannelMapSP
    validate(dir + "/chanmap_golden_cold.txt");  // 13-col, dune::PD2HDChannelMapSP

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "dune-daq-codec channel-map golden (vs DUNE detchannelmaps) OK\n";
    return 0;
}
