// SPDX-License-Identifier: GPL-3.0-or-later
//
// downloader.hpp — download helper for the install flow.
//
// curl is vendored under third_party/curl (HTTP-only, TLS disabled) and
// statically linked into the binary: downloads work out of the box on
// any environment, with no runtime libcurl required on the device.

#pragma once

#include <string>

namespace downloader {

// curl is compiled in; downloading is always available.
bool available();

// Downloads `url` into `dest` (follows redirects, fails on HTTP >= 400,
// aborts when stalled: <1 KiB/s for 30 s). Returns true on success;
// on failure `err` carries a short Chinese reason.
bool download_to_file(const std::string& url,
                      const std::string& dest,
                      std::string& err);

} // namespace downloader
