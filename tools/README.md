# tools — dev-only fixture generation (NOT part of the package build)

`gen_golden_wibeth.cpp` produces the committed WIBEth golden fixtures
(`test/fixtures/wibeth_fragment.bin`, `wibeth_golden.bin`) used by the
reference-free `test_golden` test. It is the ONLY place that uses DUNE's own
code (`reference/fddetdataformats/.../WIBEthFrame.hpp`, header-only) as the
independent oracle — it is a one-off dev tool, NOT a CMake target, so the
package build/tests never depend on `reference/`.

Compile + run ad-hoc (after the libs are built):

    g++ -std=c++20 \
      -I source/dune-daq-hdf -I source/dune-daq-codec -I source/dune-daq-types \
      -I reference/fddetdataformats/include -I reference/detdataformats/include \
      -I local/include \
      source/dune-daq-codec/tools/gen_golden_wibeth.cpp -o /tmp/gen \
      builds/dune-daq-hdf/libdune_daq_hdf.so builds/dune-daq-codec/libdune_daq_codec.so \
      -L local/lib -lhdf5 -Wl,-rpath,local/lib -Wl,-rpath,builds/dune-daq-hdf -Wl,-rpath,builds/dune-daq-codec
    /tmp/gen <daq.hdf5> source/dune-daq-codec/test/fixtures 2

The committed fixtures came from a PDHD WIBEth fragment
(np04hd run027980, /TriggerRecord00008.0000/.../Detector_Readout_0x00000064_WIBEth),
first 2 frames. The decoder was also cross-checked live against DUNE get_adc over
all frames of PDHD and PDVD fragments (786k ADCs, 0 mismatches).

## gen_golden_chanmap.cpp — channel-map golden (ddm-3j8.1.16)

Produces the committed channel-map golden fixtures
(`test/fixtures/chanmap_golden_warm.txt`, `chanmap_golden_cold.txt`) used by the
reference-free `test_channel_map_golden` test. The oracle is DUNE's OWN
channel-map classes `dune::TPCChannelMapSP` (12-col warm) and
`dune::PD2HDChannelMapSP` (13-col cold) — these are framework-free (plain stdlib,
namespace `dune`), unlike the cetlib/ers/logging-coupled
`dunedaq::detchannelmaps::TPCChannelMap` plugin base — so no DUNE stack is needed.
It pulls authoritative (online-key -> offline) pairs from each map (reverse lookup
by offline channel + a forward round-trip check) over the vendored tables.

Compile + run ad-hoc:

    g++ -std=c++20 -O1 -I reference/detchannelmaps/src \
      source/dune-daq-codec/tools/gen_golden_chanmap.cpp \
      reference/detchannelmaps/src/TPCChannelMapSP.cpp \
      reference/detchannelmaps/src/PD2HDChannelMapSP.cpp -o /tmp/genmap
    # committed lean sample (~1024 keys/schema):
    /tmp/genmap source/dune-daq-codec/test/fixtures source/dune-daq-codec/data/channelmaps 1000
    # exhaustive (every row; pass max_samples=0):
    /tmp/genmap source/dune-daq-codec/test/fixtures source/dune-daq-codec/data/channelmaps 0

The committed fixtures are a ~1024-key/schema stride sample of
pd2vd/PD2VDTPCChannelMap_v2.txt (warm) and pd2hd/PD2HDChannelMap_v6.txt (cold).
OnlineOfflineChannels was also validated EXHAUSTIVELY against DUNE over every row
of both maps (12288 warm + 10240 cold = 22528 keys, 0 mismatches), including the
cold-map wib = slot+1 transform.
