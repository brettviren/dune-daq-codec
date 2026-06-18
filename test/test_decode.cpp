// WIBEth decode tests (ddm-3j8.1.1). Internal-consistency: build a synthetic
// WIBEth fragment with a known ADC pattern using a spec-mirroring encoder, then
// decode and verify every value, the shape, and the stream metadata. (Golden
// validation against real DUNE ADCs is ddm-3j8.1.8.)

#include "dune_daq_codec/Decode.hpp"
#include "dune_daq_codec/FormatDescriptor.hpp"

#include "dune_daq_types/DAQEthHeader.hpp"
#include "dune_daq_types/FragmentHeader.hpp"
#include "dune_daq_types/FragmentType.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace dune_daq_codec;

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++fails;
}

// Inverse of the decoder's extract: pack one LSB-first `bits`-wide ADC value.
static void set_adc(std::byte* adc_region, unsigned wps, unsigned sample, unsigned channel,
                    unsigned bits, std::uint16_t val)
{
    const std::size_t base_word = static_cast<std::size_t>(sample) * wps;
    const unsigned bit_lo = channel * bits;
    const unsigned wi = bit_lo / 64;
    const unsigned first = bit_lo % 64;
    const std::uint64_t mask = (std::uint64_t{1} << bits) - 1;
    std::byte* wp = adc_region + (base_word + wi) * 8;

    std::uint64_t w;
    std::memcpy(&w, wp, 8);
    w &= ~(mask << first);
    w |= (static_cast<std::uint64_t>(val) & mask) << first;
    std::memcpy(wp, &w, 8);
    if (64 - first < bits) {
        const unsigned used = 64 - first;
        std::uint64_t w2;
        std::memcpy(&w2, wp + 8, 8);
        w2 &= ~(mask >> used);
        w2 |= (static_cast<std::uint64_t>(val) & mask) >> used;
        std::memcpy(wp + 8, &w2, 8);
    }
}

static std::uint16_t pattern(unsigned c, unsigned s, std::size_t f)
{
    return static_cast<std::uint16_t>((c * 131u + s * 7u + static_cast<unsigned>(f) * 3u) & 0x3FFFu);
}

int main()
{
    const auto& d = *descriptor_for(dune_daq::FragmentType::kWIBEth);
    const std::size_t hdr = sizeof(dune_daq::FragmentHeader);
    const std::size_t N = 3;  // frames
    const unsigned wps = d.n_channels * d.bits_per_adc / d.word_bits;  // 14

    std::vector<std::byte> buf(hdr + N * d.frame_bytes, std::byte{0});

    // FragmentHeader
    dune_daq::FragmentHeader h{};
    h.fragment_header_marker = dune_daq::FragmentHeader::s_fragment_header_marker;
    h.version = 5;
    h.size = buf.size();
    h.fragment_type = static_cast<dune_daq::fragment_type_t>(dune_daq::FragmentType::kWIBEth);
    std::memcpy(buf.data(), &h, sizeof(h));

    // Per-frame DAQEthHeader + ADC pattern.
    for (std::size_t f = 0; f < N; ++f) {
        std::byte* frame = buf.data() + hdr + f * d.frame_bytes;
        dune_daq::DAQEthHeader eh{};
        eh.version = 5; eh.det_id = 12; eh.crate_id = 300; eh.slot_id = 2; eh.stream_id = 7;
        eh.reserved = 0; eh.seq_id = 0; eh.block_length = 0;
        eh.timestamp = 0x1122334455667788ull + f;
        std::memcpy(frame, &eh, sizeof(eh));
        std::byte* adc = frame + d.frame_header_bytes;
        for (unsigned s = 0; s < d.n_samples; ++s)
            for (unsigned c = 0; c < d.n_channels; ++c)
                set_adc(adc, wps, s, c, d.bits_per_adc, pattern(c, s, f));
    }

    // Decode via explicit descriptor.
    auto dec = decode(d, std::span<const std::byte>(buf));
    check(dec.adc.n_channels == 64, "n_channels == 64");
    check(dec.adc.n_ticks == N * 64, "n_ticks == N*64");

    bool all = true;
    for (std::size_t f = 0; f < N && all; ++f)
        for (unsigned s = 0; s < 64 && all; ++s)
            for (unsigned c = 0; c < 64; ++c)
                if (dec.adc.at(c, f * 64 + s) != static_cast<std::int16_t>(pattern(c, s, f))) {
                    all = false;
                    std::cerr << "  mismatch c=" << c << " s=" << s << " f=" << f << "\n";
                    break;
                }
    check(all, "every ADC round-trips (decode == encoded pattern)");

    // Metadata from the first frame's DAQEthHeader.
    check(dec.meta.det_id == 12 && dec.meta.crate_id == 300 && dec.meta.slot_id == 2 &&
              dec.meta.stream_id == 7 && dec.meta.version == 5,
          "stream metadata decoded");
    check(dec.meta.first_timestamp == 0x1122334455667788ull, "first timestamp decoded");

    // Auto-select descriptor from the FragmentHeader type.
    auto dec2 = decode(std::span<const std::byte>(buf));
    check(dec2.adc.n_ticks == N * 64 && dec2.adc.at(63, 0) == dec.adc.at(63, 0),
          "decode(auto) selects WIBEth by fragment_type");

    // A misaligned fragment is rejected.
    {
        std::vector<std::byte> bad(hdr + 7201, std::byte{0});
        std::memcpy(bad.data(), &h, sizeof(h));
        bool threw = false;
        try { decode(d, std::span<const std::byte>(bad)); }
        catch (const std::runtime_error&) { threw = true; }
        check(threw, "decode throws on a frame-misaligned fragment");
    }

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "dune-daq-codec decode OK\n";
    return 0;
}
