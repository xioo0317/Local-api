// SPDX-License-Identifier: GPL-3.0-or-later
//
// version.hpp — Server version, configuration constants, and version info.
//
// Single source of truth for versioning. Bump SERVER_VERSION on each release.
// Frontend UI can GET /version to check for updates.

#pragma once

#include <string>

namespace local_api {

// Bump on each release
inline constexpr const char* SERVER_VERSION = "1.2.1";

// Debug master switch (hardcoded). CLI debug entry (`local_api debug ...`)
// is alive only while this is true. Flip to false before a release build
// to shut the debug entry off everywhere — no other change needed.
inline constexpr bool kDebugEnabled = true;

// API protocol version (bump when request/response format changes)
inline constexpr const char* API_VERSION    = "2.0";

inline constexpr const char* APP_NAME       = "Local-api";

// Server listen port (compile-time overridable via -DLOCAL_API_PORT=xxxx)
#ifndef LOCAL_API_PORT
inline constexpr int SERVER_PORT = 8080;
#else
inline constexpr int SERVER_PORT = LOCAL_API_PORT;
#endif

// Output paths for root detection result
inline constexpr const char* OUTPUT_DIR  = "/data/local/tmp/coverRoot";
inline constexpr const char* OUTPUT_FILE = "/data/local/tmp/coverRoot/root_detect.json";

} // namespace local_api
