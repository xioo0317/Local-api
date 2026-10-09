// SPDX-License-Identifier: GPL-3.0-or-later
//
// detector.hpp — Manager-level kernel root detector.
//
// Handshake-based detection for KernelSU, APatch, Magisk, SusFS.
// For KernelSU, also reports the mode (e.g. "lkm-bundled").
//
// This is a pure detection library — call run_all() to get results.
// The HTTP layer calls this from tools.cpp (detect tool).

#pragma once

#include <string>
#include <cstdint>
#include <signal.h>  // siginfo_t

namespace ksu {
struct get_info_cmd;
}  // namespace ksu

namespace ksu_detector {

// --- Result types ---

enum class KernelType {
    Unknown,
    None,
    KernelSU,
    KernelPatch,
    Magisk,
    Mixed,
};

struct KsuResult {
    bool present = false;
    std::string mode_str;  // "lkm-bundled", "lkm", "built-in", "late-load", "legacy-prctl"
};

struct ApResult {
    bool present = false;
};

struct MagiskResult {
    bool present = false;
};

struct SelinuxResult {
    std::string state = "unknown";  // "Enforcing" / "Permissive" / "unknown"
};

struct SusfsResult {
    bool detected = false;
};

struct DetectResult {
    KernelType type = KernelType::None;
    SelinuxResult selinux;
    KsuResult ksu;
    ApResult  ap;
    MagiskResult magisk;
    SusfsResult susfs;

    // Final root mode, resolved by run_all() after all probes:
    //   KernelSU_LKM   — KernelSU running in LKM mode
    //   KernelSU_SUSFS — KernelSU with SusFS present
    //   KernelSU_PE    — KernelSU with SELinux Permissive
    //   Apatch         — APatch detected
    //   Magisk         — Magisk detected
    //   KernelSU       — KernelSU without any of the above
    //   none           — nothing detected
    std::string mode = "none";
};

// Print a DetectResult as plain text ("key : value" per line) to stdout.
// SELinux line reports the getenforce state (Enforcing / Permissive).
void print_detector_result(const DetectResult& r);

// --- Detector class ---

class Detector {
public:
    Detector();
    ~Detector();
    DetectResult run_all();

private:
    KsuResult probe_ksu();
    ApResult  probe_apatch();
    MagiskResult probe_magisk();
    SusfsResult probe_susfs();
    SelinuxResult probe_selinux();

    // SIGSYS handler for catching unsupported syscalls
    bool sigsys_installed_ = false;
    static volatile bool g_sigsys_hit_;
    static void sigsys_handler(int sig, siginfo_t* si, void* ctx);
    void install_sigsys();
    void uninstall_sigsys();

    // KernelSU helpers
    int ksu_install_fd();
    bool ksu_do_get_info(int fd, ksu::get_info_cmd& info);

    // APatch helpers
    long ap_raw_call(const char* key, uint16_t cmd,
                     long arg3 = 0, long arg4 = 0,
                     long arg5 = 0, long arg6 = 0);
    bool ap_hello(const char* key);

    // Magisk helpers
    bool magisk_find_socket(std::string& out_path);
    bool magisk_probe_daemon(const std::string& socket_path,
                             uint32_t& out_version_code,
                             std::string& out_version_str);
};

} // namespace ksu_detector
