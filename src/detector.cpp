// SPDX-License-Identifier: GPL-3.0-or-later
//
// detector.cpp — Manager-level detector implementation.
//
// Handshake-based detection for KernelSU, APatch, Magisk, and SusFS.
// This is a pure detection library — tools.cpp calls Detector::run_all().

#include "detector.hpp"
#include "ksu_uapi.hpp"
#include "apatch_uapi.hpp"
#include "magisk_uapi.hpp"
#include "susfs_uapi.hpp"

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>

namespace ksu_detector {

// ===========================================================================
// SIGSYS handler
// ===========================================================================

volatile bool Detector::g_sigsys_hit_ = false;

void Detector::sigsys_handler(int sig, siginfo_t* si, void* ctx) {
    (void)sig;
    if (!si || si->si_code != 1) return;
    g_sigsys_hit_ = true;
#if defined(__aarch64__)
    ucontext_t* uc = static_cast<ucontext_t*>(ctx);
    uc->uc_mcontext.regs[0] = static_cast<uint64_t>(-EPERM);
#elif defined(__x86_64__)
    ucontext_t* uc = static_cast<ucontext_t*>(ctx);
    uc->uc_mcontext.gregs[REG_RAX] = static_cast<long>(-EPERM);
#else
    (void)ctx;
#endif
}

void Detector::install_sigsys() {
    if (sigsys_installed_) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigsys_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, nullptr);
    sigsys_installed_ = true;
}

void Detector::uninstall_sigsys() {
    if (!sigsys_installed_) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSYS, &sa, nullptr);
    sigsys_installed_ = false;
}

Detector::Detector() = default;
Detector::~Detector() {
    if (sigsys_installed_) uninstall_sigsys();
}

// ===========================================================================
// KernelSU
// ===========================================================================

int Detector::ksu_install_fd() {
    g_sigsys_hit_ = false;
    int fd = -1;
    long r = syscall(SYS_reboot,
                     static_cast<unsigned int>(ksu::INSTALL_MAGIC1),
                     static_cast<unsigned int>(ksu::INSTALL_MAGIC2),
                     0, &fd);
    if (r < 0 && fd < 0) return -1;
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return -1; }
    return fd;
}

bool Detector::ksu_do_get_info(int fd, ksu::get_info_cmd& info) {
    memset(&info, 0, sizeof(info));
    if (ioctl(fd, ksu::IOCTL_GET_INFO, &info) < 0) return false;
    return info.version != 0;
}

KsuResult Detector::probe_ksu() {
    KsuResult result;
    install_sigsys();
    int fd = ksu_install_fd();
    if (fd >= 0) {
        ksu::get_info_cmd info;
        if (ksu_do_get_info(fd, info)) {
            result.present = true;

            if (info.flags & ksu::GET_INFO_FLAG_LATE_LOAD) {
                result.mode_str = "late-load";
            } else if (info.flags & ksu::GET_INFO_FLAG_LKM) {
                result.mode_str = (info.flags & ksu::GET_INFO_FLAG_BUNDLED) ? "lkm-bundled" : "lkm";
            } else {
                result.mode_str = "built-in";
            }
        }
        close(fd);
    }
    if (!result.present) {
        long legacy_ver = prctl(ksu::LEGACY_MAGIC, 0, 0, 0, 0);
        if (legacy_ver >= 0) {
            result.present = true;
            result.mode_str = "legacy-prctl";
        }
    }
    return result;
}

// ===========================================================================
// APatch
// ===========================================================================

long Detector::ap_raw_call(const char* key, uint16_t cmd, long arg3, long arg4, long arg5, long arg6) {
    if (!key || !key[0]) return -EINVAL;
    uint64_t ver_cmd = apatch::make_ver_and_cmd(0, cmd);
    return syscall(static_cast<long>(apatch::NR_SUPERCALL), key, static_cast<long>(ver_cmd), arg3, arg4, arg5, arg6);
}

bool Detector::ap_hello(const char* key) {
    if (!key || !key[0]) return false;
    long ret = ap_raw_call(key, apatch::SUPERCALL_HELLO);
    return static_cast<uint32_t>(ret) == apatch::HELLO_MAGIC;
}

ApResult Detector::probe_apatch() {
    ApResult result;
    const char* key = "su";
    if (ap_hello(key)) {
        result.present = true;
    }
    return result;
}

// ===========================================================================
// Magisk
// ===========================================================================

bool Detector::magisk_find_socket(std::string& out_path) {
    auto is_sock = [](const char* p) {
        struct stat st;
        return stat(p, &st) == 0 && S_ISSOCK(st.st_mode);
    };

    if (is_sock(magisk::DSOCKET_PATH))      { out_path = magisk::DSOCKET_PATH; return true; }
    if (is_sock(magisk::SBIN_SOCKET_PATH))  { out_path = magisk::SBIN_SOCKET_PATH; return true; }

    FILE* f = fopen("/proc/net/unix", "r");
    if (f) {
        char line[512];
        if (fgets(line, sizeof(line), f)) {
            while (fgets(line, sizeof(line), f)) {
                char* sp = strrchr(line, ' ');
                if (!sp) continue;
                while (*sp == ' ' || *sp == '\n' || *sp == '\r') { *sp = '\0'; if (sp == line) break; --sp; }
                char* path = strrchr(line, ' ');
                if (!path) continue;
                ++path;
                if (*path == '@' || !*path) continue;
                if (strstr(path, magisk::SOCKET_DIR_MARKER)) {
                    out_path = path;
                    fclose(f);
                    return true;
                }
            }
        }
        fclose(f);
    }

    if (is_sock(magisk::LEGACY_SOCKET_PATH)) { out_path = magisk::LEGACY_SOCKET_PATH; return true; }
    return false;
}

bool Detector::magisk_probe_daemon(const std::string& socket_path, uint32_t& out_version_code, std::string& out_version_str) {
    auto do_connect = [](const std::string& path) -> int {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) return -1;
        struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        bool is_abstract = path[0] == '@';
        std::string name = is_abstract ? path.substr(1) : path;
        if (name.size() >= sizeof(addr.sun_path)) { close(sock); return -1; }
        if (is_abstract) {
            addr.sun_path[0] = '\0';
            memcpy(addr.sun_path + 1, name.c_str(), name.size());
        } else {
            memcpy(addr.sun_path, name.c_str(), name.size());
        }
        socklen_t addr_len = static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) +
                             (is_abstract ? 1 : 0) + name.size());
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), addr_len) < 0) {
            close(sock); return -1;
        }
        return sock;
    };

    auto write_i32 = [](int sock, int32_t v) -> bool {
        ssize_t n = write(sock, &v, sizeof(v));
        return n == static_cast<ssize_t>(sizeof(v));
    };
    auto read_i32 = [](int sock, int32_t& v) -> bool {
        ssize_t n = read(sock, &v, sizeof(v));
        return n == static_cast<ssize_t>(sizeof(v));
    };

    {
        int sock = do_connect(socket_path);
        if (sock < 0) return false;
        if (!write_i32(sock, magisk::DaemonRequestCode::CHECK_VERSION_CODE)) { close(sock); return false; }
        int32_t resp = -1;
        if (!read_i32(sock, resp)) { close(sock); return false; }
        if (resp != magisk::DaemonRespondCode::RESP_OK) { close(sock); return false; }
        int32_t ver = 0;
        if (!read_i32(sock, ver) || ver <= 0) { close(sock); return false; }
        out_version_code = static_cast<uint32_t>(ver);
        close(sock);
    }

    {
        int sock = do_connect(socket_path);
        if (sock < 0) return true;
        bool ok = write_i32(sock, magisk::DaemonRequestCode::CHECK_VERSION);
        int32_t resp = -1;
        if (!ok || !read_i32(sock, resp) || resp != magisk::DaemonRespondCode::RESP_OK) { close(sock); return true; }
        int32_t len = 0;
        if (read_i32(sock, len) && len > 0 && len < 4096) {
            std::vector<char> buf(static_cast<size_t>(len) + 1, 0);
            ssize_t total = 0;
            while (total < len) {
                ssize_t n = read(sock, buf.data() + total, static_cast<size_t>(len - total));
                if (n <= 0) break;
                total += n;
            }
            buf[total] = '\0';
            out_version_str = std::string(buf.data());
        }
        close(sock);
    }
    return true;
}

MagiskResult Detector::probe_magisk() {
    MagiskResult result;
    std::string sock_path;
    if (magisk_find_socket(sock_path)) {
        uint32_t ver_code = 0;
        std::string ver_str;
        if (magisk_probe_daemon(sock_path, ver_code, ver_str)) {
            result.present = true;
        }
    }
    return result;
}

// ===========================================================================
// SusFS (independent kernel-level handshake)
// ===========================================================================

SusfsResult Detector::probe_susfs() {
    SusfsResult result;
    install_sigsys();

    {
        susfs::version_cmd v2;
        memset(&v2, 0, sizeof(v2));
        v2.err = susfs::ERR_CMD_NOT_SUPPORTED;
        g_sigsys_hit_ = false;
        syscall(SYS_reboot,
                static_cast<unsigned int>(susfs::OPTION_MAGIC),
                susfs::REBOOT_MAGIC2,
                static_cast<unsigned long>(susfs::CMD_SHOW_VERSION),
                &v2);
        if (!g_sigsys_hit_ && v2.err == 0) {
            result.detected = true;
            return result;
        }
    }

    {
        char ver_buf[susfs::VERSION_STR_LEN];
        memset(ver_buf, 0, sizeof(ver_buf));
        int error = susfs::PRCTL_ERR_UNSUPPORTED;
        prctl(static_cast<int>(susfs::OPTION_MAGIC),
              static_cast<unsigned long>(susfs::CMD_SHOW_VERSION),
              ver_buf, NULL, &error);
        if (error == 0) {
            result.detected = true;
            return result;
        }
    }

    return result;
}

// ===========================================================================
// run_all
// ===========================================================================

DetectResult Detector::run_all() {
    DetectResult result;
    result.ksu = probe_ksu();
    result.ap = probe_apatch();
    result.magisk = probe_magisk();
    result.susfs = probe_susfs();
    result.selinux = probe_selinux();

    int count = 0;
    if (result.ksu.present) count++;
    if (result.ap.present) count++;
    if (result.magisk.present) count++;
    if (count > 1) result.type = KernelType::Mixed;
    else if (result.ksu.present) result.type = KernelType::KernelSU;
    else if (result.ap.present) result.type = KernelType::KernelPatch;
    else if (result.magisk.present) result.type = KernelType::Magisk;
    else result.type = KernelType::None;
    return result;
}

// ===========================================================================
// SELinux probe (sh -> getenforce)
// ===========================================================================

SelinuxResult Detector::probe_selinux() {
    // `getenforce` (toybox on Android) prints "Enforcing" or "Permissive".
    // Run it through the shell; missing command / unreadable -> "unknown".
    SelinuxResult out;
    out.state = "unknown";
    FILE* pipe = ::popen("getenforce 2>/dev/null", "r");
    if (pipe == nullptr) return out;
    char buf[64] = {};
    const bool got = std::fgets(buf, sizeof(buf), pipe) != nullptr;
    ::pclose(pipe);
    if (!got) return out;
    const std::string line(buf);
    if (line.find("Enforcing") != std::string::npos)       out.state = "Enforcing";
    else if (line.find("Permissive") != std::string::npos) out.state = "Permissive";
    return out;
}

// ===========================================================================
// Plain-text result printing (CLI debug entry)
// ===========================================================================

namespace {

const char* kernel_type_name(KernelType t) {
    switch (t) {
        case KernelType::Unknown:     return "unknown";
        case KernelType::None:        return "none";
        case KernelType::KernelSU:    return "kernelsu";
        case KernelType::KernelPatch: return "kernelpatch";
        case KernelType::Magisk:      return "magisk";
        case KernelType::Mixed:       return "mixed";
    }
    return "unknown";
}

const char* present(bool v) { return v ? "present" : "not present"; }

} // anonymous namespace

void print_detector_result(const DetectResult& r) {
    std::cout << "detector:\n"
              << "  detected : " << kernel_type_name(r.type) << "\n"
              << "  SELinux  : " << r.selinux.state << "\n"
              << "  kernelsu : " << present(r.ksu.present);
    if (r.ksu.present) {
        std::cout << " (mode: " << r.ksu.mode_str << ")";
    }
    std::cout << "\n"
              << "  apatch   : " << present(r.ap.present) << "\n"
              << "  magisk   : " << present(r.magisk.present) << "\n"
              << "  susfs    : " << present(r.susfs.detected) << "\n";
}

} // namespace ksu_detector
