// SPDX-License-Identifier: GPL-3.0-or-later
//
// downloader.cpp — download helper backed by the vendored, statically
// linked curl (prebuilt libcurl.a). See downloader.hpp for the contract.

#include "downloader.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <cstddef>
#include <string>

namespace downloader {
namespace {

// One-time global initialization (magic static => thread-safe).
const bool kGlobalInit = [] {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}();

std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb,
                     void* userdata) {
    std::FILE* f = static_cast<std::FILE*>(userdata);
    return std::fwrite(ptr, size, nmemb, f);
}

}  // namespace

bool available() { return kGlobalInit; }

bool download_to_file(const std::string& url,
                      const std::string& dest,
                      std::string& err) {
    err.clear();
    if (!kGlobalInit) {
        err = "下载失败：curl 初始化失败";
        return false;
    }

    std::FILE* f = std::fopen(dest.c_str(), "wb");
    if (!f) {
        err = "下载失败：无法创建目标文件 " + dest;
        return false;
    }

    CURL* easy = curl_easy_init();
    if (!easy) {
        std::fclose(f);
        std::remove(dest.c_str());
        err = "下载失败：curl 会话创建失败";
        return false;
    }

    char errbuf[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(easy, CURLOPT_USERAGENT, "Local-api");
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(easy, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(easy, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, errbuf);

    const CURLcode rc = curl_easy_perform(easy);
    curl_easy_cleanup(easy);
    std::fclose(f);

    if (rc != CURLE_OK) {
        std::remove(dest.c_str());
        err = std::string("下载失败：") +
              (errbuf[0] ? errbuf : curl_easy_strerror(rc)) +
              " (code " + std::to_string(static_cast<int>(rc)) + ")";
        return false;
    }
    return true;
}

}  // namespace downloader
