// Version-robustness / invariant tests (ddm-3j8.1.7.3). Structural only — no
// ADC decoding, no golden values, no HDF5: validate fragments described purely
// by (descriptor, authoritative total byte length, FragmentHeader).

#include "dune_daq_codec/FormatDescriptor.hpp"

#include "dune_daq_types/FragmentHeader.hpp"
#include "dune_daq_types/FragmentType.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace dune_daq_codec;
using dune_daq::FragmentType;

static int fails = 0;
static void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "ok   " : "FAIL ") << what << "\n";
    if (!ok) ++fails;
}

static dune_daq::FragmentHeader make_header(FragmentType t)
{
    dune_daq::FragmentHeader h{};
    h.fragment_header_marker = dune_daq::FragmentHeader::s_fragment_header_marker;
    h.version = 5;  // the observed (advisory) version
    h.fragment_type = static_cast<dune_daq::fragment_type_t>(t);
    return h;
}

int main()
{
    const std::size_t hdr = sizeof(dune_daq::FragmentHeader);  // 72

    // --- registry / descriptor self-consistency ---
    const auto* w = descriptor_for(FragmentType::kWIBEth);
    check(w != nullptr, "descriptor_for(kWIBEth)");
    check(w && w->frame_bytes == 7200, "WIBEth frame_bytes == 7200");
    check(w && w->adc_bytes_per_frame() == 7168, "WIBEth adc_bytes_per_frame == 7168");
    check(w && w->self_consistent(), "WIBEth descriptor self-consistent");
    check(descriptor_for(FragmentType::kDAPHNE) == nullptr, "kDAPHNE not yet supported -> nullptr");

    // --- valid fragment: 99 frames ---
    {
        auto h = make_header(FragmentType::kWIBEth);
        auto r = validate(*w, hdr + 99 * 7200, h);
        check(r.ok && r.n_frames == 99, "valid WIBEth fragment -> 99 frames");
        check(require_valid(*w, hdr + 99 * 7200, h) == 99, "require_valid returns frame count");
    }

    // --- misaligned payload (not a whole number of frames) -> fail ---
    {
        auto h = make_header(FragmentType::kWIBEth);
        auto r = validate(*w, hdr + 7201, h);
        check(!r.ok, "misaligned payload rejected");
        bool threw = false;
        try { require_valid(*w, hdr + 7201, h); } catch (const std::runtime_error&) { threw = true; }
        check(threw, "require_valid throws on misalignment");
    }

    // --- bad marker -> fail (validity is the marker, not the version) ---
    {
        auto h = make_header(FragmentType::kWIBEth);
        h.fragment_header_marker = 0xdeadbeef;
        auto r = validate(*w, hdr + 99 * 7200, h);
        check(!r.ok, "bad marker rejected");
    }

    // --- declared version is advisory: a 'wrong' version still validates ---
    {
        auto h = make_header(FragmentType::kWIBEth);
        h.version = 99;  // unknown/newer version
        auto r = validate(*w, hdr + 7 * 7200, h);
        check(r.ok && r.n_frames == 7, "unknown declared version still validates (advisory)");
    }

    // --- auto_detect: WIBEth & TDEEth share frame_bytes -> ambiguous (nullptr) ---
    {
        auto h = make_header(FragmentType::kWIBEth);
        check(auto_detect(hdr + 99 * 7200, h) == nullptr,
              "auto_detect ambiguous when formats share frame_bytes (name must disambiguate)");
    }

    if (fails) { std::cerr << fails << " failures\n"; return 1; }
    std::cout << "dune-daq-codec invariants OK\n";
    return 0;
}
