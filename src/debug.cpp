// SPDX-License-Identifier: GPL-3.0-or-later
//
// debug.cpp — CLI debug entry, standalone diagnostics.
//
// `local_api debug detector` runs detector.cpp (KSU / APatch / Magisk /
// SusFS handshake probing) by itself and prints the result as JSON.
// The whole entry is dead the moment kDebugEnabled is flipped to false.

#include "debug.hpp"
#include "detector.hpp"
#include "version.hpp" // APP_NAME / SERVER_VERSION / kDebugEnabled

#include <iostream>
#include <string>

namespace debug {

int run(const std::string& sub) {
    // Master switch: hardcoded in version.hpp, turned off for releases.
    if (!local_api::kDebugEnabled) {
        std::cout << "[debug] disabled in this build (kDebugEnabled = false)\n";
        return 1;
    }

    std::cout << local_api::APP_NAME << " debug v" << local_api::SERVER_VERSION << "\n";

    if (sub == "detector") {
        // Standalone run of the detector library: handshake-probe
        // KernelSU / APatch / Magisk / SusFS and print the result.
        ksu_detector::Detector detector;
        const ksu_detector::DetectResult result = detector.run_all();
        std::cout << ksu_detector::result_to_json_string(result) << "\n";
        return 0;
    }

    if (!sub.empty()) {
        std::cout << "[debug] unknown subcommand: '" << sub << "'\n";
    }
    std::cout << "usage: local_api debug <subcommand>\n"
              << "subcommands:\n"
              << "  detector  run detector.cpp standalone, print result JSON\n";
    return sub.empty() ? 0 : 1;
}

} // namespace debug
