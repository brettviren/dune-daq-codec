#ifndef DUNE_DAQ_CODEC_DECODE_HPP
#define DUNE_DAQ_CODEC_DECODE_HPP

// Decode a DUNE DAQ detector-readout Fragment of packed ADC frames into a dense
// 2-D int16 array (ddm-3j8.1.1).
//
// Generic over the FormatDescriptor: the packed ADCs are LSB-first
// `bits_per_adc`-bit fields in little-endian `word_bits`-wide words,
// time-sample-major within each frame (see docs/wibeth-format.md). One fragment
// of N frames yields a [n_channels][N * n_samples] block: channel `c` is
// consistent across frames; ticks are frame samples concatenated in time order.
//
// Pure: input is raw fragment bytes; output is std::vector<int16_t> + a small
// metadata struct. The DAQ->detector channel map and Arrow/WCT framing are the
// bridge's concern (ddm-3j8.1.5), not here.

#include "dune_daq_codec/FormatDescriptor.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace dune_daq_codec {

/// Dense ADC block, row-major [channel][tick]: adcs[channel*n_ticks + tick].
struct DenseAdc {
    unsigned n_channels{0};
    std::size_t n_ticks{0};
    std::vector<std::int16_t> adcs;

    std::int16_t at(unsigned channel, std::size_t tick) const
    {
        return adcs[static_cast<std::size_t>(channel) * n_ticks + tick];
    }
};

/// Per-stream metadata read from the first frame's DAQEthHeader.
struct StreamMeta {
    unsigned version{0};
    unsigned det_id{0};
    unsigned crate_id{0};
    unsigned slot_id{0};
    unsigned stream_id{0};
    std::uint64_t first_timestamp{0};
};

struct DecodedFragment {
    DenseAdc adc;
    StreamMeta meta;
};

/// Decode a whole fragment (the bytes INCLUDING the 72-byte FragmentHeader)
/// using descriptor `d`. Validates structural invariants first (throws
/// std::runtime_error on violation). `fragment_bytes.size()` is the
/// authoritative fragment length (the HDF5 dataset byte count).
DecodedFragment decode(const FormatDescriptor& d, std::span<const std::byte> fragment_bytes);

/// As above, selecting the descriptor from the FragmentHeader's fragment_type;
/// throws if the type is unsupported.
DecodedFragment decode(std::span<const std::byte> fragment_bytes);

}  // namespace dune_daq_codec

#endif  // DUNE_DAQ_CODEC_DECODE_HPP
