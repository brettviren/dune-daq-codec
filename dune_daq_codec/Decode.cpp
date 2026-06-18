#include "dune_daq_codec/Decode.hpp"

#include "dune_daq_types/DAQEthHeader.hpp"
#include "dune_daq_types/FragmentHeader.hpp"

#include <cstring>
#include <stdexcept>

namespace dune_daq_codec {

namespace {

// Read a little-endian 64-bit word at a byte pointer (unaligned-safe).
std::uint64_t read_word64(const std::byte* p)
{
    std::uint64_t w;
    std::memcpy(&w, p, sizeof(w));  // host is little-endian (asserted in dune-daq-types)
    return w;
}

// Extract one LSB-first `bits`-wide ADC field for (sample, channel) from a
// frame's ADC region. `words_per_sample` 64-bit words hold one sample's
// channels packed contiguously; channel c starts at bit c*bits within that
// sample's word block.
std::uint16_t extract_adc(const std::byte* adc_region, unsigned words_per_sample, unsigned sample,
                          unsigned channel, unsigned bits)
{
    const std::size_t base_word = static_cast<std::size_t>(sample) * words_per_sample;
    const unsigned bit_lo = channel * bits;
    const unsigned wi = bit_lo / 64;
    const unsigned first = bit_lo % 64;
    const std::byte* wp = adc_region + (base_word + wi) * 8;
    std::uint64_t v = read_word64(wp) >> first;
    if (64 - first < bits) {
        v |= read_word64(wp + 8) << (64 - first);
    }
    const std::uint32_t mask = (std::uint32_t{1} << bits) - 1;
    return static_cast<std::uint16_t>(v & mask);
}

}  // namespace

DecodedFragment decode(const FormatDescriptor& d, std::span<const std::byte> fragment_bytes)
{
    if (d.word_bits != 64) {
        throw std::runtime_error("dune-daq-codec: only 64-bit packing words are supported");
    }

    // Validate against the authoritative total length + the header.
    dune_daq::FragmentHeader header;
    if (fragment_bytes.size() < sizeof(header)) {
        throw std::runtime_error("dune-daq-codec: fragment shorter than FragmentHeader");
    }
    std::memcpy(&header, fragment_bytes.data(), sizeof(header));
    const std::size_t n_frames = require_valid(d, fragment_bytes.size(), header);

    const std::byte* payload = fragment_bytes.data() + sizeof(dune_daq::FragmentHeader);
    const unsigned wps = d.n_channels * d.bits_per_adc / d.word_bits;  // words per sample

    DecodedFragment result;
    DenseAdc& out = result.adc;
    out.n_channels = d.n_channels;
    out.n_ticks = n_frames * d.n_samples;
    out.adcs.resize(static_cast<std::size_t>(out.n_channels) * out.n_ticks);

    for (std::size_t f = 0; f < n_frames; ++f) {
        const std::byte* frame = payload + f * d.frame_bytes;
        const std::byte* adc_region = frame + d.frame_header_bytes;
        for (unsigned s = 0; s < d.n_samples; ++s) {
            const std::size_t tick = f * d.n_samples + s;
            for (unsigned c = 0; c < d.n_channels; ++c) {
                out.adcs[static_cast<std::size_t>(c) * out.n_ticks + tick] =
                    static_cast<std::int16_t>(extract_adc(adc_region, wps, s, c, d.bits_per_adc));
            }
        }
    }

    // Metadata from the first frame's DAQEthHeader.
    if (n_frames > 0) {
        dune_daq::DAQEthHeader eh;
        std::memcpy(&eh, payload, sizeof(eh));
        result.meta = StreamMeta{static_cast<unsigned>(eh.version),  static_cast<unsigned>(eh.det_id),
                                 static_cast<unsigned>(eh.crate_id), static_cast<unsigned>(eh.slot_id),
                                 static_cast<unsigned>(eh.stream_id), eh.timestamp};
    }
    return result;
}

DecodedFragment decode(std::span<const std::byte> fragment_bytes)
{
    dune_daq::FragmentHeader header;
    if (fragment_bytes.size() < sizeof(header)) {
        throw std::runtime_error("dune-daq-codec: fragment shorter than FragmentHeader");
    }
    std::memcpy(&header, fragment_bytes.data(), sizeof(header));
    const FormatDescriptor* d = descriptor_for(header.type());
    if (!d) {
        throw std::runtime_error("dune-daq-codec: no descriptor for fragment_type " +
                                 std::to_string(header.fragment_type));
    }
    return decode(*d, fragment_bytes);
}

}  // namespace dune_daq_codec
