// SPDX-License-Identifier: GPL-3.0-or-later
//
// main.cpp — Entry point ONLY.
//
// Pulls in version.hpp (version + config constants) and starts the
// service through router::run(). All dispatch lives in router.cpp,
// all tool logic in tools.cpp.

#include "router.hpp"
#include "version.hpp"
#include <iostream>

int main() {
    return router::run();
}
