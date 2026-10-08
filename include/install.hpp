// SPDX-License-Identifier: GPL-3.0-or-later
//
// install.hpp — One-shot initialization entry.
//
// `local_api install` runs the one-time bootstrap: verify privileges,
// create the output directory and drop an .installed marker so later
// runs short-circuit. Basically executed once.

#pragma once

namespace install {

// One-time initialization (idempotent). Returns 0 on success.
int run();

} // namespace install
