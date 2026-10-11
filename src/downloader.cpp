// SPDX-License-Identifier: GPL-3.0-or-later
//
// downloader.cpp — libcurl loaded via dlopen (no link-time dependency).
//
// Only the handful of easy-API symbols we need are resolved; the
// CURLOPT_* numeric values below are part of libcurl's stable ABI.

#include "downloader.hpp"

#include <dlfcn.h>

#include <cstdio>
#include <cstring>

namespace downloader {
namespace {

// --- libcurl ABI constants (stable across 7.x / 8.x) ---
constexpr long kCurlGlobalDefault  = 0;      // CURL_GLOBAL_DEFAULT
constexpr int  kOptUrl             = 10002;  // CURLOPT_URL (objectpoint+2)
constexpr int  kOptWriteData       = 10001;  // CURLOPT_WRITEDATA (objectpoint+1)
constexpr int  kOptWriteFunction   = 20011;  // CURLOPT_WRITEFUNCTION (functionpoint+11)
constexpr int  kOptUserAgent       = 10018;  // CURLOPT_USERAGENT (objectpoint+18)
constexpr int  kOptFailOnError     = 45;     // CURLOPT_FAILONERROR (long)
constexpr int  kOptFollowLocation  = 52;     // CURLOPT_FOLLOWLOCATION (long)
constexpr int  kOptLowSpeedLimit   = 19;     // CURLOPT_LOW_SPEED_LIMIT (long)
constexpr int  kOptLowSpeedTime    = 20;     // CURLOPT_LOW_SPEED_TIME (long)
constexpr int  kOptNoSignal        = 99;     // CURLOPT_NOSIGNAL (long)

// Candidate library names: Android device ROMs first, host last.
const char* const kLibCandidates[] = {
    "libcurl.so",                       // Android (LD_LIBRARY_PATH)
    "/system/lib64/libcurl.so",         // Android 64-bit ROM
    "/system/lib/libcurl.so",           // Android 32-bit ROM
    "/apex/com.android.conscrypt/lib64/libcurl.so",  // some APEX layouts
    "libcurl.so.4",                     // host Linux
};

void* g_lib = nullptr;   // dlopen handle
bool  g_tried = false;

using global_init_fn = int (*)(long);
using easy_init_fn   = void* (*)();
using easy_setopt_fn = int (*)(void*, int, ...);
using easy_perform_fn = int (*)(void*);
using easy_cleanup_fn = void (*)(void*);
using strerror_fn    = const char* (*)(int);

global_init_fn  f_global_init = nullptr;
easy_init_fn    f_easy_init = nullptr;
easy_setopt_fn  f_easy_setopt = nullptr;
easy_perform_fn f_easy_perform = nullptr;
easy_cleanup_fn f_easy_cleanup = nullptr;
strerror_fn     f_strerror = nullptr;

bool load() {
    if (g_tried) return g_lib != nullptr;
    g_tried = true;
    for (const char* name : kLibCandidates) {
        void* lib = ::dlopen(name, RTLD_NOW | RTLD_LOCAL);
        if (lib == nullptr) continue;
        f_global_init = reinterpret_cast<global_init_fn>(::dlsym(lib, "curl_global_init"));
        f_easy_init   = reinterpret_cast<easy_init_fn>(::dlsym(lib, "curl_easy_init"));
        f_easy_setopt = reinterpret_cast<easy_setopt_fn>(::dlsym(lib, "curl_easy_setopt"));
        f_easy_perform = reinterpret_cast<easy_perform_fn>(::dlsym(lib, "curl_easy_perform"));
        f_easy_cleanup = reinterpret_cast<easy_cleanup_fn>(::dlsym(lib, "curl_easy_cleanup"));
        f_strerror    = reinterpret_cast<strerror_fn>(::dlsym(lib, "curl_easy_strerror"));
        if (f_global_init != nullptr && f_easy_init != nullptr &&
            f_easy_setopt != nullptr && f_easy_perform != nullptr &&
            f_easy_cleanup != nullptr && f_strerror != nullptr) {
            f_global_init(kCurlGlobalDefault);
            g_lib = lib;
            return true;
        }
        ::dlclose(lib);
    }
    return false;
}

size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    return ::fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
}

} // namespace

bool available() { return load(); }

bool download_to_file(const std::string& url,
                      const std::string& dest,
                      std::string& err) {
    if (!load()) {
        err = "libcurl 不可用（设备缺少 libcurl.so）";
        return false;
    }

    FILE* out = std::fopen(dest.c_str(), "wb");
    if (out == nullptr) {
        err = "无法创建目标文件：" + dest;
        return false;
    }

    void* easy = f_easy_init();
    if (easy == nullptr) {
        std::fclose(out);
        err = "curl 句柄创建失败";
        return false;
    }

    f_easy_setopt(easy, kOptUrl, url.c_str());
    f_easy_setopt(easy, kOptWriteData, out);
    f_easy_setopt(easy, kOptWriteFunction, reinterpret_cast<void*>(&write_cb));
    f_easy_setopt(easy, kOptUserAgent, "Local-api");
    f_easy_setopt(easy, kOptFollowLocation, 1L);
    f_easy_setopt(easy, kOptFailOnError, 1L);
    f_easy_setopt(easy, kOptNoSignal, 1L);
    f_easy_setopt(easy, kOptLowSpeedLimit, 1024L);  // <1 KiB/s ...
    f_easy_setopt(easy, kOptLowSpeedTime, 30L);     // ... for 30 s -> abort

    const int rc = f_easy_perform(easy);
    f_easy_cleanup(easy);
    std::fclose(out);

    if (rc != 0) {
        std::remove(dest.c_str());  // don't leave a partial file behind
        err = std::string("下载失败：") + f_strerror(rc) + " (" + std::to_string(rc) + ")";
        return false;
    }
    return true;
}

} // namespace downloader
