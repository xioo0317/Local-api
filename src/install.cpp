// SPDX-License-Identifier: GPL-3.0-or-later
//
// install.cpp — Installation flow (runs the full flow every time).
//
// Steps:
//   1. warn when not running as root (real install steps will need it)
//   2. detection-method menu: volume up = auto detect (recommended,
//      default), volume down = manual selection; live 10s countdown,
//      auto-selects the default on non-interactive sessions, missing
//      input devices, or when the countdown reaches zero
//   3. manual path: volume-key menu to pick KernelSU / Apatch / Magisk /
//      no Root (up/down = move, power = confirm, 30s timeout -> default)
//   4. create the output directory (OUTPUT_DIR, mkdir -p style)
//
// No marker file: re-running always executes the full flow.
// Self-contained: does NOT reuse anything from tools.cpp / router.cpp.

#include "install.hpp"
#include "detector.hpp" // automatic root environment detection
#include "version.hpp" // APP_NAME / SERVER_VERSION / OUTPUT_DIR

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <linux/input.h>

namespace install {
namespace {


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


// True when the device actually carries one of the keys the menu
// consumes (power / volume up / volume down). Devices without them —
// touchscreens, sensors — are left completely untouched, so touch
// input keeps working while the menu is up.
bool device_has_watched_keys(const int fd) {
    unsigned char key_bits[KEY_MAX / 8 + 1] = {};
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) return false;
    const auto has = [&key_bits](const int code) {
        return (key_bits[code / 8] & (1u << (code % 8))) != 0;
    };
    return has(kKeyVolumeUp) || has(kKeyVolumeDown) || has(kKeyPower);
}

// Open every /dev/input/eventN that carries the watched keys and, when
// `grab` is set, take it exclusively via EVIOCGRAB: the kernel stops
// delivering those events to the system, so KEY_POWER no longer reaches
// the power manager and the screen stays on during the menu.
//
// The grab lives on the fd: volume_select releases it explicitly when
// done, and any exit path — normal return, signal or crash — closes the
// fds and the kernel drops the grab by itself, instantly restoring the
// power key.
std::vector<int> open_input_devices(const bool grab) {
    std::vector<int> fds;
    DIR* dir = ::opendir("/dev/input");
    if (dir == nullptr) return fds;
    while (const dirent* ent = ::readdir(dir)) {
        const std::string name = ent->d_name;
        if (name.rfind("event", 0) != 0) continue;
        const int fd = ::open(("/dev/input/" + name).c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (!device_has_watched_keys(fd)) {
            ::close(fd);
            continue;
        }
        if (grab && ::ioctl(fd, EVIOCGRAB, reinterpret_cast<void*>(1)) != 0) {
            // Device busy (already grabbed elsewhere): keep listening,
            // but the system will also see the keys.
            std::cout << "警告：无法独占 " << name
                      << "，系统可能仍响应其按键\n";
        }
        fds.push_back(fd);
    }
    ::closedir(dir);
    return fds;
}

void close_input_devices(std::vector<int>& fds) {
    for (const int fd : fds) {
        ::ioctl(fd, EVIOCGRAB, reinterpret_cast<void*>(0)); // explicit release
        ::close(fd);                                        // kernel drops grab anyway
    }
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
    // "> - xxx" marks the current option; each row clears its line first so
    // the whole menu can be redrawn in place (cursor moved up by the caller).
    for (size_t i = 0; i < options.size(); ++i) {
        std::cout << "\r\033[K" << (static_cast<int>(i) == sel ? "> " : "  ")
                  << "- " << options[i] << "\n";
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

    std::cout << "音量上/下键：移动选项，电源键：确认\n";
    print_menu(options, sel);

    // Non-interactive stdout (pipes, CI): never wait on keys.
    if (::isatty(STDOUT_FILENO) == 0) {
        std::cout << "非交互环境，使用默认项\n";
        return sel;
    }

    std::vector<int> fds = open_input_devices(/*grab=*/true);
    if (fds.empty()) {
        std::cout << "未找到输入设备，使用默认项\n";
        return sel;
    }
    std::cout << "已拦截电源键：菜单关闭前系统不熄屏\n";

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeout_sec);
    while (true) {
        const auto left_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 deadline - std::chrono::steady_clock::now()).count();
        if (left_ms <= 0) {
            std::cout << "超时，使用默认项\n";
            break;
        }
        const int key = wait_for_watched_key(fds, static_cast<int>(left_ms));
        if (key == -2) continue;        // unrelated input event, poll again
        if (key < 0) {                  // timeout
            std::cout << "超时，使用默认项\n";
            break;
        }
        if (key == static_cast<int>(kKeyVolumeUp)) {
            sel = (sel + count - 1) % count;
        } else if (key == static_cast<int>(kKeyVolumeDown)) {
            sel = (sel + 1) % count;
        }
        // Redraw the whole menu in place: cursor up to the first row.
        std::cout << "\r\033[" << count << "A" << std::flush;
        print_menu(options, sel);
        if (key == static_cast<int>(kKeyPower)) {
            std::cout << "已确认：" << options[sel] << "\n";
            break;
        }
    }
    close_input_devices(fds);
    return sel;
}

bool volume_choice_binary(const std::string& up_label,
                          const std::string& down_label,
                          const int timeout_sec) {
    // "自动检测 (推荐)" -> "自动检测" for the default/timeout/picked lines.
    std::string up_plain = up_label;
    const std::string suffix = " (推荐)";
    if (up_plain.size() > suffix.size() &&
        up_plain.compare(up_plain.size() - suffix.size(), suffix.size(), suffix) == 0) {
        up_plain.erase(up_plain.size() - suffix.size());
    }
    std::cout << "请选择 Root 环境检测方式：\n"
              << "请使用音量键选择：\n"
              << "  音量上键：" << up_label << "\n"
              << "  音量下键：" << down_label << "\n"
              << "  " << timeout_sec << "秒无操作：默认" << up_plain << "\n";

    // Non-interactive stdout (pipes, CI): never wait on keys.
    if (::isatty(STDOUT_FILENO) == 0) {
        std::cout << "非交互环境，默认：" << up_plain << "\n";
        return true;
    }

    // Grab devices while waiting: the power key is swallowed, so the
    // screen stays on during the choice; released on the way out and
    // automatically by the kernel on exit / crash.
    std::vector<int> fds = open_input_devices(/*grab=*/true);
    if (fds.empty()) {
        std::cout << "未找到输入设备，默认：" << up_plain << "\n";
        return true;
    }

    bool picked_up = true;
    bool timed_out = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(timeout_sec);
    while (true) {
        const auto left = std::chrono::duration_cast<std::chrono::seconds>(
                              deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            std::cout << "\r\033[K" << std::flush;
            timed_out = true;
            break;
        }
        // Live countdown line, refreshed once per second (1s poll granularity).
        std::cout << "\r\033[K  剩余 " << left << " 秒，无操作默认：" << up_plain
                  << std::flush;
        const int key = wait_for_watched_key(fds, 1000);
        if (key < 0) continue; // -1 = 1s tick, -2 = unrelated input event
        std::cout << "\r\033[K" << std::flush;
        if (key == static_cast<int>(kKeyVolumeUp)) { picked_up = true; break; }
        if (key == static_cast<int>(kKeyVolumeDown)) { picked_up = false; break; }
        // power key: swallowed by the grab on purpose, keep waiting
    }
    close_input_devices(fds);

    if (timed_out) std::cout << "超时，默认：" << up_plain << "\n";
    else std::cout << "已选择：" << (picked_up ? up_plain : down_label) << "\n";
    return picked_up;
}

int run() {
    std::cout << local_api::APP_NAME << " 安装程序 v" << local_api::SERVER_VERSION << "\n";

    if (getuid() != 0) {
        std::cout << "警告：未以 root 运行，安装可能不完整\n";
    }

    // Root environment detection mode: volume up = auto detect (default,
    // recommended), volume down = manual selection; live 10s countdown.
    std::string root_mode;
    if (volume_choice_binary("自动检测 (推荐)", "手动选择", /*timeout_sec=*/10)) {
        std::cout << "正在自动检测 Root 环境...\n";
        ksu_detector::Detector detector;
        const ksu_detector::DetectResult result = detector.run_all();
        root_mode = result.mode; // 只取最终 mode 值，完整输出留给 debug detector
        std::cout << "检测结果：" << root_mode << "\n";
    } else {
        // Two blank lines between the detection-method menu and the manual
        // selection submenu, for visual separation.
        std::cout << "\n\n";
        // The options are the mode values themselves.
        const std::vector<std::string> root_modes = {"KernelSU", "APatch", "Magisk", "none"};
        const int pick = volume_select(root_modes, /*default_index=*/0, /*timeout_sec=*/30);
        root_mode = (pick >= 0 && pick < static_cast<int>(root_modes.size()))
                        ? root_modes[static_cast<size_t>(pick)]
                        : "none";
        std::cout << "手动指定 Root 环境：" << root_mode << "\n";
    }

    std::cout << "创建输出目录：" << local_api::OUTPUT_DIR << "\n";
    if (!mkdirs(local_api::OUTPUT_DIR)) {
        std::cout << "错误：创建目录失败 " << local_api::OUTPUT_DIR
                  << "（需要 root？）\n";
        return 1;
    }

    std::cout << "安装完成：" << local_api::OUTPUT_DIR << " 已就绪\n";

    // TODO(后续开发): 真正的安装步骤（部署资源 / 配置 / 权限设置）在此追加。
    return 0;
}

} // namespace install
