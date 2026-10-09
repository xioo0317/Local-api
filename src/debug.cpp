// SPDX-License-Identifier: GPL-3.0-or-later
//
// debug.cpp — CLI debug entry, standalone diagnostics.
//
// `local_api debug detector` runs detector.cpp (KSU / APatch / Magisk /
// SusFS handshake probing) by itself and prints the result as plain,
// human-readable text — one "key : value" per line, no nested JSON, so
// no terminal can mangle the layout.
// The whole entry is dead the moment kDebugEnabled is flipped to false.

#include "debug.hpp"
#include "detector.hpp"
#include "version.hpp" // APP_NAME / SERVER_VERSION / kDebugEnabled

#include <iostream>
#include <string>

namespace debug {
namespace {

// KernelType -> readable name (kept local: debug stays self-contained,
// detector.cpp internals are not touched).
const char* kernel_type_name(ksu_detector::KernelType t) {
    switch (t) {
        case ksu_detector::KernelType::KernelSU:    return "kernelsu";
        case ksu_detector::KernelType::KernelPatch: return "kernelpatch";
        case ksu_detector::KernelType::Magisk:      return "magisk";
        case ksu_detector::KernelType::Mixed:       return "mixed";
        case ksu_detector::KernelType::Unknown:     return "unknown";
        case ksu_detector::KernelType::None:        return "none";
    }
    return "unknown";
}

const char* yes_no(bool v) { return v ? "present" : "not present"; }

void print_detector_result(const ksu_detector::DetectResult& r) {
    std::cout << "detector:\n"
              << "  detected : " << kernel_type_name(r.type) << "\n"
              << "  kernelsu : " << yes_no(r.ksu.present);
    if (r.ksu.present) {
        std::cout << " (mode: " << r.ksu.mode_str << ")";
    }
    std::cout << "\n"
              << "  apatch   : " << yes_no(r.ap.present) << "\n"
              << "  magisk   : " << yes_no(r.magisk.present) << "\n"
              << "  susfs    : " << yes_no(r.susfs.detected) << "\n";
}

} // namespace

int run(const std::string& sub) {
    // Master switch: hardcoded in version.hpp, turned off for releases.
    if (!local_api::kDebugEnabled) {
        std::cout << "[debug] disabled in this build (kDebugEnabled = false)\n";
        return 1;
    }

    std::cout << local_api::APP_NAME << " debug v" << local_api::SERVER_VERSION << "\n";

    if (sub == "detector") {
        // Standalone run of the detector library: handshake-probe
        // KernelSU / APatch / Magisk / SusFS, print plain text.
        ksu_detector::Detector detector;
        const ksu_detector::DetectResult result = detector.run_all();
        print_detector_result(result);
        return 0;
    }

    if (!sub.empty()) {
        std::cout << "[debug] unknown subcommand: '" << sub << "'\n";
    }
    std::cout << "usage: local_api debug <subcommand>\n"
              << "subcommands:\n"
              << "  detector  run detector.cpp standalone, print plain text\n";
    return sub.empty() ? 0 : 1;
}

} // namespace debug
