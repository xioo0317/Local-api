// SPDX-License-Identifier: GPL-3.0-or-later
//
// downloader.hpp — download helper for the install flow.
//
// curl is linked as a prebuilt static archive under third_party/prebuilt/
// (built by the xioo0317/curl fork CI, HTTP-only, TLS disabled): downloads
// work out of the box on any environment, with no runtime libcurl required
// on the device.

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
