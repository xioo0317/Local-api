// SPDX-License-Identifier: GPL-3.0-or-later
//
// debug.cpp — CLI debug entry, standalone diagnostics.
//
// Subcommands (thin forwards — implementation lives in the owning module):
//   debug detector          run detector.cpp standalone (printing in detector.cpp)
//   debug downloader [url]  real-download probe via downloader.cpp (no root)
//   debug <tool>            run a registered tool directly and print its JSON,
//                           e.g. `debug tool_version` (handler style) or
//                           `debug version` (registered action name)
// The whole entry is dead the moment kDebugEnabled is flipped to false.

#include "debug.hpp"
#include "detector.hpp"
#include "downloader.hpp"
#include "tools.hpp"
#include "version.hpp" // APP_NAME / SERVER_VERSION / kDebugEnabled

#include <iostream>
#include <string>

namespace debug {
namespace {

// Accept both the registered action name ("version") and the handler-style
// name ("tool_version") so CLI usage matches the code either way.
std::string normalize_tool_name(const std::string& sub) {
    if (sub.rfind("tool_", 0) == 0) return sub.substr(5);
    return sub;
}

}  // namespace

int run(const std::string& sub, const std::string& arg) {
    // Master switch: hardcoded in version.hpp, turned off for releases.
    if (!local_api::kDebugEnabled) {
        std::cout << "[debug] disabled in this build (kDebugEnabled = false)\n";
        return 1;
    }

    std::cout << local_api::APP_NAME << " debug v" << local_api::SERVER_VERSION << "\n";

    if (sub == "detector") {
        // Standalone run of the detector library; printing is owned by
        // detector.cpp (print_detector_result).
        ksu_detector::Detector detector;
        const ksu_detector::DetectResult result = detector.run_all();
        ksu_detector::print_detector_result(result);
        return 0;
    }

    if (sub == "downloader") {
        // Real-download probe; implementation lives in downloader.cpp.
        return downloader::debug_run(arg);
    }

    // Single-tool debug: run a registered tool by name, print its JSON.
    const std::string name = normalize_tool_name(sub);
    for (const auto& action : tools::list_actions()) {
        if (action.get<std::string>() == name) {
            std::cout << tools::execute(name) << "\n";
            return 0;
        }
    }

    if (!sub.empty()) {
        std::cout << "[debug] unknown subcommand: '" << sub << "'\n";
    }
    std::cout << "usage: local_api debug <subcommand> [arg]\n"
              << "subcommands:\n"
              << "  detector          run detector.cpp standalone, print plain text\n"
              << "  downloader [url]  download url (default https://m.baidu.com/) into /tmp, no root needed\n"
              << "  <tool>            run a registered tool directly: detect / version / debug\n"
              << "                    (handler style also accepted: tool_detect / tool_version / tool_debug)\n";
    return sub.empty() ? 0 : 1;
}

} // namespace debug
