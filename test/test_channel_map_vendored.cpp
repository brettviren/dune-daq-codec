// Parse the real vendored DUNE channel-map tables (ddm-3j8.1.12).
//
// Uses the files committed under data/channelmaps (NOT reference/), so it is a
// self-contained validation of OnlineOfflineChannels against real data. For each
// file it independently re-derives the online key + offline channel from the
// first data row (encoding the 12-/13-column layout knowledge here, separately
// from the class) and checks the class's lookup agrees.

#include "dune_daq_codec/OnlineOfflineChannels.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using dune_daq_codec::OnlineOfflineChannels;

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++fails;
}

// First non-comment, non-blank row's whitespace tokens.
static std::vector<std::string> first_row(const std::string& path)
{
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);) {
        const auto hash = line.find('#');
        std::string body = (hash == std::string::npos) ? line : line.substr(0, hash);
        std::istringstream ls(body);
        std::vector<std::string> tok;
        for (std::string t; ls >> t;) tok.push_back(t);
        if (!tok.empty()) return tok;
    }
    return {};
}

static unsigned u(const std::string& s) { return static_cast<unsigned>(std::stoul(s)); }

// Validate a 13-column cold-electronics file.
static void check_cold(const std::string& path)
{
    OnlineOfflineChannels m(path);
    check(m.schema() == OnlineOfflineChannels::Schema::cold_electronics_13,
          path + ": 13-col cold schema");
    check(m.size() > 0, path + ": rows parsed");

    auto t = first_row(path);  // offlchan crate APAName wib link ... wibframechan
    check(t.size() == 13, path + ": first row has 13 columns");
    if (t.size() == 13) {
        const unsigned offl = u(t[0]), crate = u(t[1]), wib = u(t[3]), link = u(t[4]),
                       wfc = u(t[12]);
        // Query uses slot (= wib-1), stream (= link), channel (= wibframechan); det ignored.
        check(m.offline(/*det=*/0, crate, wib - 1, link, wfc) == static_cast<int>(offl),
              path + ": first-row online key -> its offline channel");
    }
}

// Validate a 12-column warm-electronics file.
static void check_warm(const std::string& path)
{
    OnlineOfflineChannels m(path);
    check(m.schema() == OnlineOfflineChannels::Schema::warm_electronics_12,
          path + ": 12-col warm schema");
    check(m.size() > 0, path + ": rows parsed");

    auto t = first_row(path);  // offlchan detid detelement crate slot stream streamchan ...
    check(t.size() == 12, path + ": first row has 12 columns");
    if (t.size() == 12) {
        const unsigned offl = u(t[0]), detid = u(t[1]), crate = u(t[3]), slot = u(t[4]),
                       stream = u(t[5]), schan = u(t[6]);
        check(m.offline(detid, crate, slot, stream, schan) == static_cast<int>(offl),
              path + ": first-row online key -> its offline channel");
    }
}

int main()
{
    const std::string dir = CHANNELMAPS_DIR;

    // ProtoDUNE-HD (cold, 13-col) and ProtoDUNE-VD bottom (cold, 13-col).
    check_cold(dir + "/pd2hd/PD2HDChannelMap_v6.txt");
    check_cold(dir + "/pd2vd/PD2VDBottomTPCChannelMap_v1.txt");

    // ProtoDUNE-VD (warm, 12-col) and ProtoDUNE-VD top (warm, 12-col).
    check_warm(dir + "/pd2vd/PD2VDTPCChannelMap_v2.txt");
    check_warm(dir + "/pd2vd/PD2VDTopTPCChannelMap_v2.txt");

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "vendored channel maps OK\n";
    return 0;
}
