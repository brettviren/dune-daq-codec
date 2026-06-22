// Tests for OnlineOfflineChannels (ddm-3j8.1.13).
//
// Reference-free: writes small synthetic ChannelMap*.txt tables that mirror the
// real 12-column (warm) and 13-column (cold) layouts, then exercises schema
// auto-detection, the unified online->offline query (including the cold-map
// wib = slot+1 quirk and the warm-map detid-in-key behaviour), the common-column
// reverse lookup, and error handling.

#include "dune_daq_codec/OnlineOfflineChannels.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using dune_daq_codec::OnlineOfflineChannels;

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++fails;
}

static void write_file(const std::string& path, const std::string& content)
{
    std::ofstream os(path);
    os << content;
}

int main()
{
    // --- 13-column cold-electronics table -----------------------------------
    // offlchan crate APAName wib link femb_on_link cebchan
    //          plane chan_in_plane femb asic asicchan wibframechan
    const std::string cold_path = "oochan_cold.txt";
    write_file(cold_path,
               "# a cold-electronics (HD / VD-bottom) channel map\n"
               "0   1 APA_P02SU 2 0 0 0   0 0 1 1 0   0\n"
               "1   1 APA_P02SU 2 0 0 1   0 1 1 1 1   1\n"
               "100 1 APA_P02SU 3 1 0 0   2 0 5 3 0   200\n");

    OnlineOfflineChannels cold(cold_path);
    check(cold.schema() == OnlineOfflineChannels::Schema::cold_electronics_13,
          "cold: detected 13-column schema");
    check(cold.size() == 3, "cold: 3 rows (comment skipped)");
    // slot -> wib (= slot+1), stream -> link, channel -> wibframechan; det ignored.
    check(cold.offline(/*det=*/99, /*crate=*/1, /*slot=*/1, /*stream=*/0, /*chan=*/0) == 0,
          "cold: (crate1, slot1->wib2, link0, wibframechan0) -> offline 0; det ignored");
    check(cold.offline(0, 1, 1, 0, 1) == 1, "cold: wibframechan 1 -> offline 1");
    check(cold.offline(0, 1, 2, 1, 200) == 100,
          "cold: (slot2->wib3, link1, wibframechan200) -> offline 100");
    check(cold.offline(0, 1, 0, 0, 0) == OnlineOfflineChannels::invalid_channel,
          "cold: wrong slot (wib1) -> invalid");

    // --- 12-column warm-electronics table -----------------------------------
    // offlchan detid detelement crate slot stream streamchan
    //          plane chan_in_plane femb asic asicchan
    const std::string warm_path = "oochan_warm.txt";
    write_file(warm_path,
               "0   10 3 1 0 0 0   0 0 1 1 0\n"
               "1   10 3 1 0 0 1   0 1 1 1 1\n"
               "500 11 5 2 3 0 7   2 0 5 3 0\n");

    OnlineOfflineChannels warm(warm_path);
    check(warm.schema() == OnlineOfflineChannels::Schema::warm_electronics_12,
          "warm: detected 12-column schema");
    check(warm.size() == 3, "warm: 3 rows");
    check(warm.offline(/*det=*/10, /*crate=*/1, /*slot=*/0, /*stream=*/0, /*chan=*/0) == 0,
          "warm: (detid10, crate1, slot0, stream0, streamchan0) -> offline 0");
    check(warm.offline(10, 1, 0, 0, 1) == 1, "warm: streamchan 1 -> offline 1");
    check(warm.offline(11, 2, 3, 0, 7) == 500, "warm: second detid row -> offline 500");
    // detid IS part of the key: same crate/slot/stream/chan, wrong detid -> miss.
    check(warm.offline(99, 1, 0, 0, 0) == OnlineOfflineChannels::invalid_channel,
          "warm: detid is part of the key (wrong detid -> invalid)");

    // --- common-column reverse lookup ---------------------------------------
    auto ci = warm.info(500);
    check(ci.valid && ci.crate == 2 && ci.plane == 2 && ci.chan_in_plane == 0 &&
              ci.femb == 5 && ci.asic == 3 && ci.asicchan == 0,
          "warm: info(500) returns common columns");
    check(!warm.info(123456).valid, "warm: info() of unknown offline -> invalid");

    auto cci = cold.info(100);
    check(cci.valid && cci.crate == 1 && cci.plane == 2 && cci.femb == 5,
          "cold: info(100) returns common columns");

    // --- error handling ------------------------------------------------------
    bool threw = false;
    try {
        OnlineOfflineChannels bad("definitely_no_such_file.txt");
    }
    catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "missing file throws");

    threw = false;
    write_file("oochan_bad.txt", "1 2 3 4 5\n");  // 5 columns
    try {
        OnlineOfflineChannels bad("oochan_bad.txt");
    }
    catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "unrecognized column count throws");

    if (fails) {
        std::cerr << fails << " failures\n";
        return 1;
    }
    std::cout << "OnlineOfflineChannels OK\n";
    return 0;
}
