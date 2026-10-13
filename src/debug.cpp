// SPDX-License-Identifier: GPL-3.0-or-later
//
// debug.cpp — CLI debug entry, standalone diagnostics.
//
// `local_api debug detector` runs detector.cpp by itself (printing lives in
// detector.cpp). `local_api debug downloader [url]` exercises the vendored
// curl download path (no root required): it fetches `url` (default
// https://m.baidu.com/) into /tmp and reports the saved size, so the
// prebuilt libcurl.a link and the TLS handshake can be verified from a
// plain shell.
// The whole entry is dead the moment kDebugEnabled is flipped to false.

#include "debug.hpp"
#include "detector.hpp"
#include "downloader.hpp"
#include "version.hpp" // APP_NAME / SERVER_VERSION / kDebugEnabled

#include <cstdio>
#include <iostream>
#include <string>

namespace debug {
namespace {

// Default probe target: a real HTTPS site with redirects.
constexpr const char* kDefaultUrl = "https://m.baidu.com/";
constexpr const char* kOutPath = "/tmp/local_api_debug_download.out";

int run_downloader(const std::string& url_in) {
    const std::string url = url_in.empty() ? kDefaultUrl : url_in;
    std::cout << "url: " << url << "\n";
    if (!downloader::available()) {
        std::cout << "result: FAIL\nreason: curl 初始化失败\n";
        return 1;
    }
    std::remove(kOutPath);
    std::string err;
    if (!downloader::download_to_file(url, kOutPath, err)) {
        std::cout << "result: FAIL\nreason: " << err << "\n";
        return 1;
    }
    std::FILE* f = std::fopen(kOutPath, "rb");
    if (!f) {
        std::cout << "result: FAIL\nreason: 下载成功但无法读取目标文件\n";
        return 1;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fclose(f);
    std::cout << "result: OK\nsaved: " << kOutPath << "\nsize: " << size
              << " bytes\n";
    return 0;
}

}  // namespace

int run(const std::string& sub, const std::string& arg) {
    // Master switch: hardcoded in version.hpp, turned off for releases.
    if (!local_api::kDebugEnabled) {
        std::cout << "[debug] disabled in this build (kDebugEnabled = false)\n";
        return 1;
    }

    std::cout << local_api::APP_NAME << " debug v" << local_api::SERVER_VERSION << "\n";

    if (sub == "detector") {
        // Standalone run of the detector library; printing is owned by
        // detector.cpp (print_detector_result).
        ksu_detector::Detector detector;
        const ksu_detector::DetectResult result = detector.run_all();
        ksu_detector::print_detector_result(result);
        return 0;
    }

    if (sub == "downloader") {
        // Real-download probe of the vendored libcurl.a (no root needed).
        return run_downloader(arg);
    }

    if (!sub.empty()) {
        std::cout << "[debug] unknown subcommand: '" << sub << "'\n";
    }
    std::cout << "usage: local_api debug <subcommand> [arg]\n"
              << "subcommands:\n"
              << "  detector          run detector.cpp standalone, print plain text\n"
              << "  downloader [url]  download url (default " << kDefaultUrl
              << ") into /tmp, no root needed\n";
    return sub.empty() ? 0 : 1;
}

} // namespace debug
