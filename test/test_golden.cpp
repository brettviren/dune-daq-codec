// Golden validation of the WIBEth decoder (ddm-3j8.1.8).
//
// Loads a committed fixture extracted from a REAL DUNE DAQ file plus the GOLDEN
// ADC values produced by DUNE's OWN WIBEthFrame::get_adc (see
// tools/gen_golden_wibeth.cpp), decodes the fragment with our codec, and asserts
// every ADC matches. This test depends ONLY on the committed fixtures and our
// codec — NOT on the reference clones (project rule).

#include "dune_daq_codec/Decode.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR must be defined"
#endif

static std::vector<std::byte> read_file(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) { std::cerr << "cannot open " << path << "\n"; std::exit(2); }
    const auto n = static_cast<std::size_t>(in.tellg());
    std::vector<std::byte> b(n);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(b.data()), n);
    return b;
}

int main()
{
    const std::string dir = FIXTURES_DIR;
    auto frag = read_file(dir + "/wibeth_fragment.bin");
    auto goldb = read_file(dir + "/wibeth_golden.bin");
    const std::int16_t* golden = reinterpret_cast<const std::int16_t*>(goldb.data());
    const std::size_t n_golden = goldb.size() / sizeof(std::int16_t);

    auto dec = dune_daq_codec::decode(std::span<const std::byte>(frag));

    int fails = 0;
    auto check = [&](bool ok, const std::string& w) {
        std::cout << (ok ? "ok   " : "FAIL ") << w << "\n";
        if (!ok) ++fails;
    };

    check(dec.adc.n_channels == 64, "decoded 64 channels");
    check(dec.adc.n_ticks * dec.adc.n_channels == n_golden, "golden size matches decoded shape");

    long mism = 0;
    for (unsigned c = 0; c < dec.adc.n_channels; ++c)
        for (std::size_t t = 0; t < dec.adc.n_ticks; ++t)
            if (dec.adc.at(c, t) != golden[static_cast<std::size_t>(c) * dec.adc.n_ticks + t]) ++mism;
    check(mism == 0, "every decoded ADC == DUNE golden value");
    if (mism) std::cerr << "  " << mism << " / " << n_golden << " mismatches\n";

    if (fails) return 1;
    std::cout << "dune-daq-codec golden (WIBEth, real DUNE data) OK\n";
    return 0;
}
