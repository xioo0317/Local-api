// SPDX-License-Identifier: GPL-3.0-or-later
//
// install.cpp — One-shot initialization (basically run once).
//
// Steps:
//   1. warn when not running as root (real install steps will need it)
//   2. short-circuit when the .installed marker already exists
//   3. confirm via the volume-key menu (up/down = move, power = confirm;
//      auto-selects the default on non-interactive sessions, missing
//      input devices, or after a 30s timeout)
//   4. create the output directory (OUTPUT_DIR, mkdir -p style)
//   5. write the marker file (version + timestamp)
//
// Idempotent by design: re-running is a no-op once installed.
// Self-contained: does NOT reuse anything from tools.cpp / router.cpp.

#include "install.hpp"
#include "version.hpp" // APP_NAME / SERVER_VERSION / OUTPUT_DIR

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <linux/input.h>

namespace install {
namespace {

constexpr const char* kMarkerFile = "/.installed";

// Linux key codes (linux/input-event-codes.h).
constexpr __u16 kKeyVolumeDown = 114;
constexpr __u16 kKeyVolumeUp   = 115;
constexpr __u16 kKeyPower      = 116;

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

// Open every /dev/input/eventN so key presses can be watched.
std::vector<int> open_input_devices() {
    std::vector<int> fds;
    DIR* dir = ::opendir("/dev/input");
    if (dir == nullptr) return fds;
    while (const dirent* ent = ::readdir(dir)) {
        const std::string name = ent->d_name;
        if (name.rfind("event", 0) != 0) continue;
        const int fd = ::open(("/dev/input/" + name).c_str(), O_RDONLY | O_NONBLOCK);
        if (fd >= 0) fds.push_back(fd);
    }
    ::closedir(dir);
    return fds;
}

void close_input_devices(std::vector<int>& fds) {
    for (const int fd : fds) ::close(fd);
    fds.clear();
}

// Block until one of the watched keys is pressed. Returns the key code,
// or -1 on timeout / error. Other keys are ignored.
int wait_for_watched_key(const std::vector<int>& fds, int timeout_ms) {
    std::vector<pollfd> pfds(fds.size());
    for (size_t i = 0; i < fds.size(); ++i) {
        pfds[i].fd = fds[i];
        pfds[i].events = POLLIN;
        pfds[i].revents = 0;
    }
    const int r = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), timeout_ms);
    if (r <= 0) return -1; // timeout or poll error
    for (const auto& p : pfds) {
        if ((p.revents & POLLIN) == 0) continue;
        input_event ev {};
        if (::read(p.fd, &ev, sizeof(ev)) != static_cast<ssize_t>(sizeof(ev))) continue;
        if (ev.type != EV_KEY || ev.value != 1) continue; // key press only
        if (ev.code == kKeyVolumeUp || ev.code == kKeyVolumeDown || ev.code == kKeyPower) {
            return static_cast<int>(ev.code);
        }
    }
    return -2; // woke up for an unrelated event, caller re-polls
}

void print_menu(const std::vector<std::string>& options, const int sel) {
    for (size_t i = 0; i < options.size(); ++i) {
        std::cout << "[menu] " << (static_cast<int>(i) == sel ? " > " : "   ")
                  << (i + 1) << ". " << options[i] << "\n";
    }
}

} // namespace

int volume_select(const std::vector<std::string>& options,
                  const int default_index,
                  const int timeout_sec) {
    if (options.empty()) return -1;
    int sel = (default_index >= 0 && default_index < static_cast<int>(options.size()))
                  ? default_index
                  : 0;
    const int count = static_cast<int>(options.size());

    std::cout << "[menu] volume up/down = move, power = confirm\n";
    print_menu(options, sel);
    std::cout << "[menu] selected: " << (sel + 1) << ". " << options[sel] << std::flush;

    // Non-interactive stdout (pipes, CI): never wait on keys.
    if (::isatty(STDOUT_FILENO) == 0) {
        std::cout << "\n[menu] non-interactive session, keeping default\n";
        return sel;
    }

    std::vector<int> fds = open_input_devices();
    if (fds.empty()) {
        std::cout << "\n[menu] no input device found, keeping default\n";
        return sel;
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeout_sec);
    while (true) {
        const auto left_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 deadline - std::chrono::steady_clock::now()).count();
        if (left_ms <= 0) {
            std::cout << "\n[menu] timed out, keeping default\n";
            break;
        }
        const int key = wait_for_watched_key(fds, static_cast<int>(left_ms));
        if (key == -2) continue;        // unrelated input event, poll again
        if (key < 0) {                  // timeout
            std::cout << "\n[menu] timed out, keeping default\n";
            break;
        }
        if (key == static_cast<int>(kKeyVolumeUp)) {
            sel = (sel + count - 1) % count;
        } else if (key == static_cast<int>(kKeyVolumeDown)) {
            sel = (sel + 1) % count;
        }
        // Redraw the current selection line in place.
        std::cout << "\r\033[K[menu] selected: " << (sel + 1) << ". " << options[sel]
                  << std::flush;
        if (key == static_cast<int>(kKeyPower)) {
            std::cout << "\n[menu] confirmed: " << (sel + 1) << ". " << options[sel] << "\n";
            break;
        }
    }
    close_input_devices(fds);
    return sel;
}

int run() {
    std::cout << local_api::APP_NAME << " install v" << local_api::SERVER_VERSION << "\n"
              << "one-time initialization\n";

    if (getuid() != 0) {
        std::cout << "[install] warning: not running as root, "
                     "installation may be incomplete\n";
    }

    const std::string marker = std::string(local_api::OUTPUT_DIR) + kMarkerFile;

    // Idempotent: already installed -> nothing to do (no menu).
    if (file_exists(marker)) {
        std::cout << "[install] already initialized (" << marker << ")\n"
                  << "[install] nothing to do\n";
        return 0;
    }

    // Confirmation via the volume-key menu. Auto-selects "install now"
    // on non-interactive sessions (CI / piped output) or missing devices.
    const std::vector<std::string> options = {"install now", "cancel"};
    const int choice = volume_select(options, /*default_index=*/0, /*timeout_sec=*/30);
    if (choice != 0) {
        std::cout << "[install] cancelled by user, nothing was changed\n";
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
