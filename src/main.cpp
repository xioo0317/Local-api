// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — Entry point ONLY.
//
// Pulls in version.hpp (version + config constants) and dispatches by
// argv[1]:
//
//   local_api install  — one-shot initialization (install::run(), run once)
//   local_api router   — long-running web service
//   local_api          — anything else falls into the else branch below:
//                        same as `router` (long-running web service)
//
// All dispatch lives in router.cpp, all tool logic in tools.cpp,
// one-shot setup in install.cpp.

#include "install.hpp"
#include "router.hpp"
#include "version.hpp"

#include <string>

int main(int argc, char* argv[]) {
    const std::string cmd = argc > 1 ? argv[1] : "";

    if (cmd == "install") {
        // `local_api install` — one-shot initialization, basically run once.
        return install::run();
    } else {
        // `local_api router` / no argument / anything else —
        // start the long-running web service.
        return router::run();
    }
}
