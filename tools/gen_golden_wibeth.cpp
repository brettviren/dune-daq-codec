// One-off DEV tool (NOT part of the package build): generate a small committable
// golden fixture for the WIBEth decode test, using DUNE's OWN WIBEthFrame::get_adc
// as the independent oracle. Compiled ad-hoc against the read-only reference
// headers (header-only — no DUNE stack build needed); see tools/README.md.
//
//   <built libs> + -I reference/fddetdataformats/include -I reference/detdataformats/include
//
// Emits, from the first WIBEth fragment of <daq.hdf5>:
//   test/fixtures/wibeth_fragment.bin  : FragmentHeader(72) + first K frames
//   test/fixtures/wibeth_golden.bin    : K*64*64 int16 golden ADCs, [channel][tick]
#include "dune_daq_hdf/DaqHdf5File.hpp"
#include "fddetdataformats/WIBEthFrame.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <span>
#include <vector>
int main(int argc, char** argv) {
  const int K = argc > 3 ? std::atoi(argv[3]) : 2;  // frames to capture
  dune_daq_hdf::DaqHdf5File f(argv[1]);
  const std::string outdir = argv[2];
  for (auto& fi : f.fragments(f.records().at(0))) {
    if (fi.type != dune_daq::FragmentType::kWIBEth) continue;
    auto bytes = f.read_bytes(fi.dataset_path);
    const std::size_t frag_bytes = 72 + (std::size_t)K * 7200;
    std::ofstream(outdir + "/wibeth_fragment.bin", std::ios::binary)
        .write(reinterpret_cast<const char*>(bytes.data()), frag_bytes);
    std::vector<std::int16_t> golden((std::size_t)K * 64 * 64);
    const std::size_t n_ticks = (std::size_t)K * 64;
    for (int fr = 0; fr < K; ++fr) {
      dunedaq::fddetdataformats::WIBEthFrame wf;
      std::memcpy(&wf, bytes.data() + 72 + (std::size_t)fr * 7200, sizeof(wf));
      for (int c = 0; c < 64; ++c)
        for (int s = 0; s < 64; ++s)
          golden[(std::size_t)c * n_ticks + fr * 64 + s] = (std::int16_t)wf.get_adc(c, s);
    }
    std::ofstream(outdir + "/wibeth_golden.bin", std::ios::binary)
        .write(reinterpret_cast<const char*>(golden.data()), golden.size() * sizeof(std::int16_t));
    std::printf("wrote %zu-byte fragment + %zu golden int16 (K=%d) from %s\n",
                frag_bytes, golden.size(), K, fi.dataset_path.c_str());
    return 0;
  }
  return 1;
}
