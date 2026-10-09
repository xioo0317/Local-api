// SPDX-License-Identifier: GPL-3.0-or-later
//
// debug.hpp — CLI debug entry (`local_api debug <subcommand>`).
//
// Gated by local_api::kDebugEnabled (hardcoded in version.hpp):
// with the switch off, debug::run() refuses to do anything.
// Subcommands run standalone diagnostics, e.g. `debug detector` executes
// detector.cpp by itself and prints the result.

#pragma once

#include <string>

namespace debug {

// Run a debug subcommand (argv[2]). Returns 0 on success.
int run(const std::string& sub);

} // namespace debug
