// SPDX-License-Identifier: GPL-3.0-or-later
//
// downloader.hpp — libcurl download helper for the install flow.
//
// libcurl is loaded at runtime (dlopen) instead of being linked: the
// NDK does not ship libcurl, while real devices (OnePlus / Qualcomm
// ROMs included) expose /system/lib64/libcurl.so. Host builds fall
// back to libcurl.so.4. Neither Makefile nor Android.mk needs a
// link-time dependency, so the CI stays green.

#pragma once

#include <string>

namespace downloader {

// True when a usable libcurl could be loaded in this process.
bool available();

// Downloads `url` into `dest` (follows redirects, fails on HTTP >= 400,
// aborts when stalled: <1 KiB/s for 30 s). Returns true on success;
// on failure `err` carries a short Chinese reason.
bool download_to_file(const std::string& url,
                      const std::string& dest,
                      std::string& err);

} // namespace downloader
