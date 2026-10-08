// SPDX-License-Identifier: GPL-3.0-or-later
//
// install.cpp — One-shot initialization (basically run once).
//
// Steps:
//   1. warn when not running as root (real install steps will need it)
//   2. short-circuit when the .installed marker already exists
//   3. create the output directory (OUTPUT_DIR, mkdir -p style)
//   4. write the marker file (version + timestamp)
//
// Idempotent by design: re-running is a no-op once installed.
// Self-contained: does NOT reuse anything from tools.cpp / router.cpp.

#include "install.hpp"
#include "version.hpp" // APP_NAME / SERVER_VERSION / OUTPUT_DIR

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

namespace install {
namespace {

constexpr const char* kMarkerFile = "/.installed";

// mkdir -p equivalent: create every path component of `path`.
bool mkdirs(const std::string& path) {
    if (path.empty()) return false;
    size_t pos = 0;
    while (pos < path.size()) {
        size_t next = path.find('/', pos + 1);
        if (next == std::string::npos) next = path.size();
        const std::string cur = path.substr(0, next);
        if (!cur.empty() && cur != "/") {
            if (mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) {
                return false;
            }
        }
        pos = next;
    }
    return true;
}

bool file_exists(const std::string& path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
}

} // namespace

int run() {
    std::cout << local_api::APP_NAME << " install v" << local_api::SERVER_VERSION << "\n"
              << "one-time initialization\n";

    if (getuid() != 0) {
        std::cout << "[install] warning: not running as root, "
                     "installation may be incomplete\n";
    }

    const std::string marker = std::string(local_api::OUTPUT_DIR) + kMarkerFile;

    // Idempotent: already installed -> nothing to do.
    if (file_exists(marker)) {
        std::cout << "[install] already initialized (" << marker << ")\n"
                  << "[install] nothing to do\n";
        return 0;
    }

    std::cout << "[install] creating output directory: " << local_api::OUTPUT_DIR << "\n";
    if (!mkdirs(local_api::OUTPUT_DIR)) {
        std::cout << "[install] error: failed to create " << local_api::OUTPUT_DIR
                  << " (root required?)\n";
        return 1;
    }

    std::ofstream out(marker);
    if (out) {
        const std::time_t now = std::time(nullptr);
        out << "installed_by=" << local_api::APP_NAME << "\n"
            << "version=" << local_api::SERVER_VERSION << "\n"
            << "timestamp=" << now << "\n";
    }
    out.close();
    if (!file_exists(marker)) {
        std::cout << "[install] error: failed to write marker " << marker << "\n";
        return 1;
    }

    std::cout << "[install] complete: " << local_api::OUTPUT_DIR << " ready\n";

    // TODO(后续开发): 真正的安装步骤（部署资源 / 配置 / 权限设置）在此追加。
    return 0;
}

} // namespace install
