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
