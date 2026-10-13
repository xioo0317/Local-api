// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — Entry point ONLY.
//
// Pulls in version.hpp (version + config constants) and dispatches by
// argv[1]:
//
//   local_api install  — one-shot initialization (install::run(), run once)
//   local_api debug X  — CLI debug entry (debug::run(X)), gated by the
//                        hardcoded master switch local_api::kDebugEnabled;
//                        e.g. `debug detector` runs detector.cpp standalone,
//                        `debug downloader [url]` tests the vendored curl
//   local_api router   — long-running web service
//   local_api          — anything else falls into the else branch below:
//                        same as `router` (long-running web service)
//
// All dispatch lives in router.cpp, all tool logic in tools.cpp,
// one-shot setup in install.cpp, CLI diagnostics in debug.cpp.

#include "debug.hpp"
#include "install.hpp"
#include "router.hpp"
#include "version.hpp"

#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
    // Under the KSU installer our stdout is a pipe (libsu reads it line by
    // line), and non-tty stdio defaults to FULL buffering: text stays in the
    // user-space buffer until it fills or the process exits, so the installer
    // log shows nothing while menus run. Disable buffering so every line is
    // written out immediately.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    const std::string cmd = argc > 1 ? argv[1] : "";

    if (cmd == "install") {
        // `local_api install` — one-shot initialization, basically run once.
        return install::run();
    } else if (cmd == "debug") {
        // `local_api debug <sub>` — CLI debug entry, master-switch gated.
        return debug::run(argc > 2 ? argv[2] : "",
                          argc > 3 ? argv[3] : "");
    } else {
        // `local_api router` / no argument / anything else —
        // start the long-running web service.
        return router::run();
    }
}
