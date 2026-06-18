#ifndef DUNE_DAQ_CODEC_FORMATDESCRIPTOR_HPP
#define DUNE_DAQ_CODEC_FORMATDESCRIPTOR_HPP

// Data-driven format descriptor + version-robustness invariants (ddm-3j8.1.7.3).
//
// Per ADR-001 (docs/format-descriptor-adr.md) the codec is DIY and data-driven:
// each supported detector format is a small descriptor (the parameterized
// LSB-first bit-array geometry) that one generic decode loop (ddm-3j8.1.1)
// consumes. This header defines that descriptor, a small registry, and the
// VERSION-ROBUSTNESS validation — the structural invariants a fragment must
// satisfy, derived from real PDHD+PDVD data:
//
//   * validity is the FragmentHeader marker (0x11112222), NOT the version field;
//   * the AUTHORITATIVE fragment size is the HDF5 dataset byte length, NOT the
//     unreliable FragmentHeader.size field (which can be a round buffer size or
//     even smaller than the real content);
//   * the frame-count invariant is (dataset_bytes - sizeof(FragmentHeader)) %
//     frame_bytes == 0;
//   * declared version fields are ADVISORY (recorded, never trusted to fail).
//
// Header-only, depends only on dune-daq-types.

#include "dune_daq_types/FragmentHeader.hpp"
#include "dune_daq_types/FragmentType.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dune_daq_codec {

/// One detector format's packing parameters (a "generation" of a format).
struct FormatDescriptor {
    dune_daq::FragmentType fragment_type;  ///< the type this descriptor handles
    std::string_view name;                 ///< human/debug name (e.g. "WIBEth")

    unsigned word_bits;            ///< ADC packing word width (64)
    unsigned bits_per_adc;         ///< bits per ADC sample (14 WIBEth)
    unsigned n_channels;           ///< channels per frame
    unsigned n_samples;            ///< time samples (ticks) per frame
    std::size_t frame_header_bytes;///< per-frame header bytes (DAQEthHeader + frame hdr)
    std::size_t frame_bytes;       ///< total per-frame bytes (header + packed ADC)

    /// Bytes of packed ADC per frame, from the geometry.
    constexpr std::size_t adc_bytes_per_frame() const
    {
        return static_cast<std::size_t>(n_samples) * (n_channels * bits_per_adc / word_bits) *
               (word_bits / 8);
    }
    /// The geometry is internally consistent (whole ADC words; frame_bytes adds up).
    constexpr bool self_consistent() const
    {
        return word_bits != 0 && (n_channels * bits_per_adc) % word_bits == 0 &&
               frame_bytes == frame_header_bytes + adc_bytes_per_frame();
    }
};

// Registry of supported descriptors. WIBEth is confirmed (7200 B/frame, 64×64×14)
// against real data. TDEEth shares the same 7200 B/frame (its payloads are exact
// multiples of 7200); its internal channel/sample geometry is PROVISIONAL here
// (assumed WIBEth-like) and must be confirmed against reference TDEEthFrame when
// TDEEth decode is implemented — only frame_bytes matters for the structural
// invariants below.
inline constexpr FormatDescriptor descriptors[] = {
    {dune_daq::FragmentType::kWIBEth, "WIBEth", 64, 14, 64, 64, 32, 7200},
    {dune_daq::FragmentType::kTDEEth, "TDEEth", 64, 14, 64, 64, 32, 7200},  // geometry provisional
};

/// The descriptor for a FragmentType, or nullptr if unsupported.
constexpr const FormatDescriptor* descriptor_for(dune_daq::FragmentType t)
{
    for (const auto& d : descriptors) {
        if (d.fragment_type == t) return &d;
    }
    return nullptr;
}

struct ValidationResult {
    bool ok{true};
    std::size_t n_frames{0};
    std::vector<std::string> problems;
};

/// Validate a fragment against a descriptor. `total_bytes` MUST be the
/// authoritative HDF5 dataset byte length (not FragmentHeader.size). `header` is
/// the fragment's FragmentHeader. Records (does not fail on) a declared
/// fragment_type that disagrees with the descriptor.
inline ValidationResult validate(const FormatDescriptor& d, std::size_t total_bytes,
                                 const dune_daq::FragmentHeader& header)
{
    ValidationResult r;
    auto bad = [&](std::string m) { r.ok = false; r.problems.push_back(std::move(m)); };

    if (!d.self_consistent()) bad("descriptor '" + std::string(d.name) + "' is not self-consistent");
    if (!header.valid_marker()) bad("bad FragmentHeader marker (expected 0x11112222)");

    if (total_bytes < sizeof(dune_daq::FragmentHeader)) {
        bad("fragment smaller than FragmentHeader");
    }
    else if (d.frame_bytes == 0) {
        bad("descriptor frame_bytes == 0");
    }
    else {
        const std::size_t payload = total_bytes - sizeof(dune_daq::FragmentHeader);
        if (payload % d.frame_bytes != 0) {
            bad("payload (" + std::to_string(payload) + " B) is not a whole number of " +
                std::to_string(d.frame_bytes) + "-byte frames");
        }
        else {
            r.n_frames = payload / d.frame_bytes;
        }
    }
    // Advisory: declared type should match (the dataset name already selected it).
    if (header.type() != d.fragment_type) {
        r.problems.push_back("note: declared fragment_type differs from descriptor (advisory)");
    }
    return r;
}

/// Fail loudly: throws std::runtime_error with the joined problems if invalid.
inline std::size_t require_valid(const FormatDescriptor& d, std::size_t total_bytes,
                                 const dune_daq::FragmentHeader& header)
{
    auto r = validate(d, total_bytes, header);
    if (!r.ok) {
        std::string msg = "dune-daq-codec: invalid " + std::string(d.name) + " fragment:";
        for (const auto& p : r.problems) msg += " [" + p + "]";
        throw std::runtime_error(msg);
    }
    return r.n_frames;
}

/// Stretch: when the fragment_type is untrustworthy, pick the descriptor whose
/// structural invariants hold. Returns the UNIQUE passing descriptor, or nullptr
/// if none or several pass (e.g. WIBEth and TDEEth share frame_bytes, so they are
/// indistinguishable by structure alone — the dataset name must disambiguate).
inline const FormatDescriptor* auto_detect(std::size_t total_bytes,
                                           const dune_daq::FragmentHeader& header)
{
    const FormatDescriptor* found = nullptr;
    for (const auto& d : descriptors) {
        auto r = validate(d, total_bytes, header);
        if (r.ok) {
            if (found) return nullptr;  // ambiguous
            found = &d;
        }
    }
    return found;
}

}  // namespace dune_daq_codec

#endif  // DUNE_DAQ_CODEC_FORMATDESCRIPTOR_HPP
