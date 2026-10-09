// SPDX-License-Identifier: GPL-3.0-or-later
//
// install.hpp — One-shot initialization entry + volume-key menu tool.
//
// `local_api install` runs the one-time bootstrap: verify privileges,
// confirm via the volume-key menu, create the output directory and drop
// an .installed marker so later runs short-circuit.

#pragma once

#include <string>
#include <vector>

namespace install {

// Volume-key selection menu: volume up/down moves the selection,
// power confirms. Returns the selected index, or -1 when `options`
// is empty. Falls back to `default_index` (no interaction needed)
// when stdout is not a tty, no input device exists, or on timeout.
int volume_select(const std::vector<std::string>& options,
                  int default_index = 0,
                  int timeout_sec = 30);

// One-time initialization (idempotent). Returns 0 on success.
int run();

} // namespace install
