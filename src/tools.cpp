// SPDX-License-Identifier: GPL-3.0-or-later
//
// tools.cpp — Tool function implementations.
//
// Each tool is called by router.cpp via execute(action, params).
// Detection is one tool; the feature tools configure the launcher icon,
// SusFS hidden paths, hidden app list, auth key and module hash.
//
// NOTE: every feature tool below requires root privileges (getuid() == 0),
// because it runs `pm enable/disable` or writes under /data/local/tmp as root.

#include "tools.hpp"
#include "detector.hpp"
#include "version.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cctype>

#include <unistd.h>
#include <sys/stat.h>

using json = nlohmann::json;

namespace tools {

namespace {

// Launcher-alias component toggled by tool_hide_icon.
constexpr const char* ICON_COMPONENT = "com.coverRoot/.LauncherAlias";

// Config files persisted under the coverRoot working directory.
constexpr const char* SUSFS_PATHS_FILE = "/data/local/tmp/coverRoot/susfs_paths.json";
constexpr const char* HIDDEN_APPS_FILE = "/data/local/tmp/coverRoot/hidden_apps.json";
constexpr const char* AUTH_KEY_FILE    = "/data/local/tmp/coverRoot/auth_key";
constexpr const char* MODULE_HASH_FILE = "/data/local/tmp/coverRoot/module.sha256";

// True when the process is running as root.
bool running_as_root() {
    return getuid() == 0;
}

// True if s contains characters usable for shell injection / redirection.
// All untrusted strings that may reach a shell are checked against this set.
bool contains_dangerous_chars(const std::string& s) {
    static const std::string bad =
        std::string(";|&$`(){}\n\r<>\\") + "\"'";
    return s.find_first_of(bad) != std::string::npos;
}

// Basic Android package-name check: letters/digits/'.'/'_' only, has a dot.
bool is_valid_package(const std::string& pkg) {
    if (pkg.empty() || pkg.size() > 255) return false;
    if (pkg.find('.') == std::string::npos) return false;
    for (char c : pkg) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_')) {
            return false;
        }
    }
    return true;
}

// Basic hex hash check (sha256 = 64 chars; accept 32..128 to stay lenient).
bool is_valid_hex_hash(const std::string& h) {
    if (h.size() < 32 || h.size() > 128) return false;
    for (char c : h) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

// Write a string to a file; returns true on success.
bool write_text_file(const std::string& path, const std::string& content) {
    mkdir(local_api::OUTPUT_DIR, 0755);
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;
    out << content;
    out.close();
    return out.good();
}

json make_error(const std::string& message) {
    return json{{"status", "error"}, {"message", message}};
}

} // namespace

// ===========================================================================
// Dispatcher
// ===========================================================================

std::string execute(const std::string& action, const json& params) {
    if (action == "detect")  return tool_detect(params);
    if (action == "version") return tool_version(params);
    if (action == "debug")   return tool_debug(params);

    // Placeholder tools
    if (action == "sysinfo") return tool_sysinfo(params);
    if (action == "modules") return tool_modules(params);
    if (action == "config")  return tool_config(params);

    // New feature tools (require root)
    if (action == "hide_icon")     return tool_hide_icon(params);
    if (action == "susfs_setup")   return tool_susfs_setup(params);
    if (action == "hide_app_list") return tool_hide_app_list(params);
    if (action == "update_key")    return tool_update_key(params);
    if (action == "set_hash")      return tool_set_hash(params);

    json err = {
        {"status", "error"},
        {"error", "unknown action: " + action},
        {"available_actions", list_actions()}
    };
    return err.dump();
}

json list_actions() {
    return json::array({
        "detect", "version", "debug",
        "sysinfo", "modules", "config",
        "hide_icon", "susfs_setup", "hide_app_list",
        "update_key", "set_hash"
    });
}

// ===========================================================================
// Tool: detect — Root detection (KernelSU / APatch / Magisk / SusFS)
// ===========================================================================

std::string tool_detect(const json&) {
    ksu_detector::Detector detector;
    auto result = detector.run_all();
    std::string detect_json = ksu_detector::result_to_json_string(result);

    // Write result to file
    mkdir(local_api::OUTPUT_DIR, 0755);
    std::ofstream out(local_api::OUTPUT_FILE, std::ios::trunc);

    json reply;
    if (out.is_open()) {
        out << detect_json;
        out.close();
        if (out.good()) {
            reply = {{"status", "ok"}, {"result_file", local_api::OUTPUT_FILE}};
        } else {
            reply = {{"status", "error"}, {"error", "failed to write result file"}};
        }
    } else {
        reply = {{"status", "error"}, {"error", "failed to write result file"}};
    }

    // Embed the detection result inline as well
    reply["result"] = json::parse(detect_json);
    return reply.dump();
}

// ===========================================================================
// Tool: version — Server version + API version
// ===========================================================================

std::string tool_version(const json&) {
    json j;
    j["status"] = "ok";
    j["server_version"] = local_api::SERVER_VERSION;
    j["api_version"] = local_api::API_VERSION;
    j["app_name"] = local_api::APP_NAME;
    return j.dump();
}

// ===========================================================================
// Tool: debug — Tool info, versions, dry-run output
// ===========================================================================

std::string tool_debug(const json&) {
    json j;
    j["status"] = "ok";
    j["server_version"] = local_api::SERVER_VERSION;
    j["api_version"] = local_api::API_VERSION;
    j["output_dir"] = local_api::OUTPUT_DIR;
    j["output_file"] = local_api::OUTPUT_FILE;
    j["pid"] = getpid();
    j["is_root"] = running_as_root();

    // List registered tools
    j["tools"] = list_actions();

    // Dry-run: execute detection and include result
    ksu_detector::Detector detector;
    auto result = detector.run_all();
    j["detect_dry_run"] = json::parse(ksu_detector::result_to_json_string(result));

    // Tool status
    json ts;
    ts["detect"]        = {{"status", "ready"}, {"description", "Root detection (KSU/APatch/Magisk/SusFS)"}};
    ts["version"]       = {{"status", "ready"}, {"description", "Server version info"}};
    ts["debug"]         = {{"status", "ready"}, {"description", "Debug info + dry-run"}};
    ts["sysinfo"]       = {{"status", "placeholder"}, {"description", "System information (not implemented)"}};
    ts["modules"]       = {{"status", "placeholder"}, {"description", "Module management (not implemented)"}};
    ts["config"]        = {{"status", "placeholder"}, {"description", "Configuration (not implemented)"}};
    ts["hide_icon"]     = {{"status", "ready"}, {"description", "Hide/restore launcher icon (requires root)"}};
    ts["susfs_setup"]   = {{"status", "ready"}, {"description", "Configure SusFS hidden paths (requires root)"}};
    ts["hide_app_list"] = {{"status", "ready"}, {"description", "Configure hidden app list (requires root)"}};
    ts["update_key"]    = {{"status", "ready"}, {"description", "Update authentication key (requires root)"}};
    ts["set_hash"]      = {{"status", "ready"}, {"description", "Set module integrity hash (requires root)"}};
    j["tool_status"] = ts;

    return j.dump();
}

// ===========================================================================
// Placeholder tools — implement when needed
// ===========================================================================

std::string tool_sysinfo(const json&) {
    json reply = {
        {"status", "ok"},
        {"message", "not implemented yet"},
        {"hint", "will return device model, Android version, kernel info, etc."}
    };
    return reply.dump();
}

std::string tool_modules(const json&) {
    json reply = {
        {"status", "ok"},
        {"message", "not implemented yet"},
        {"hint", "will list installed kernel modules, enable/disable"}
    };
    return reply.dump();
}

std::string tool_config(const json&) {
    json reply = {
        {"status", "ok"},
        {"message", "not implemented yet"},
        {"hint", "will read/write server configuration"}
    };
    return reply.dump();
}

// ===========================================================================
// Feature tools — require root privileges
// ===========================================================================

// ---------------------------------------------------------------------------
// tool_hide_icon — hide / restore the coverRoot launcher icon.
//
// params: {"enabled": true|false}  (true = hide, false = restore)
// Runs `pm disable` / `pm enable` on com.coverRoot/.LauncherAlias.
// Requires root.
// ---------------------------------------------------------------------------
std::string tool_hide_icon(const json& params) {
    if (!params.contains("enabled") || !params["enabled"].is_boolean()) {
        return make_error("missing or invalid boolean parameter: enabled").dump();
    }
    bool enabled = params["enabled"].get<bool>();

    if (!running_as_root()) {
        json e = make_error("root privileges required");
        e["enabled"] = enabled;
        return e.dump();
    }

    // enabled == true  -> hide icon   (pm disable)
    // enabled == false -> restore icon (pm enable)
    std::string cmd =
        std::string(enabled ? "pm disable " : "pm enable ") +
        ICON_COMPONENT + " >/dev/null 2>&1";

    int rc = std::system(cmd.c_str());

    json reply;
    if (rc == 0) {
        reply["status"]  = "ok";
        reply["message"] = enabled ? "icon hidden" : "icon restored";
    } else {
        reply["status"]  = "error";
        reply["message"] = "pm command failed (exit code " + std::to_string(rc) + ")";
    }
    reply["enabled"] = enabled;
    return reply.dump();
}

// ---------------------------------------------------------------------------
// tool_susfs_setup — configure SusFS hidden paths in one shot.
//
// params: {"paths": ["/abs/path1", "/abs/path2", ...]}
// Validates and persists the path list consumed by the SusFS setup flow.
// Requires root.
// ---------------------------------------------------------------------------
std::string tool_susfs_setup(const json& params) {
    if (!params.contains("paths") || !params["paths"].is_array()) {
        return make_error("missing or invalid array parameter: paths").dump();
    }

    json sanitized = json::array();
    for (const auto& p : params["paths"]) {
        if (!p.is_string()) {
            return make_error("every entry in paths must be a string").dump();
        }
        std::string path = p.get<std::string>();
        if (path.empty() || path.front() != '/') {
            return make_error("path must be absolute: " + path).dump();
        }
        if (path.find("..") != std::string::npos || contains_dangerous_chars(path)) {
            return make_error("path contains illegal characters: " + path).dump();
        }
        sanitized.push_back(path);
    }

    if (!running_as_root()) {
        json e = make_error("root privileges required");
        e["configured_paths"] = sanitized;
        return e.dump();
    }

    if (!write_text_file(SUSFS_PATHS_FILE, sanitized.dump(2))) {
        return make_error("failed to write SusFS path configuration").dump();
    }

    // Applying the list to the live SusFS kernel interface is ROM-specific;
    // the persisted config above is the input consumed during SusFS setup.
    json reply = {
        {"status", "ok"},
        {"message", "SusFS paths configured"},
        {"configured_paths", sanitized}
    };
    return reply.dump();
}

// ---------------------------------------------------------------------------
// tool_hide_app_list — configure which packages root is hidden from.
//
// params: {"packages": ["com.example.app1", "com.example.app2", ...]}
// Validates package names and persists the hidden-app list.
// Requires root.
// ---------------------------------------------------------------------------
std::string tool_hide_app_list(const json& params) {
    if (!params.contains("packages") || !params["packages"].is_array()) {
        return make_error("missing or invalid array parameter: packages").dump();
    }

    json sanitized = json::array();
    for (const auto& p : params["packages"]) {
        if (!p.is_string()) {
            return make_error("every entry must be a package-name string").dump();
        }
        std::string pkg = p.get<std::string>();
        if (!is_valid_package(pkg) || contains_dangerous_chars(pkg)) {
            return make_error("invalid package name: " + pkg).dump();
        }
        sanitized.push_back(pkg);
    }

    if (!running_as_root()) {
        json e = make_error("root privileges required");
        e["hidden_packages"] = sanitized;
        return e.dump();
    }

    if (!write_text_file(HIDDEN_APPS_FILE, sanitized.dump(2))) {
        return make_error("failed to write hidden app list").dump();
    }

    json reply = {
        {"status", "ok"},
        {"message", "hidden app list configured"},
        {"hidden_packages", sanitized}
    };
    return reply.dump();
}

// ---------------------------------------------------------------------------
// tool_update_key — update the coverRoot authentication key.
//
// params: {"key": "new_key_value"} (1..256 chars, no shell metacharacters)
// Persists the new key under the coverRoot working directory.
// Requires root.
// ---------------------------------------------------------------------------
std::string tool_update_key(const json& params) {
    if (!params.contains("key") || !params["key"].is_string()) {
        return make_error("missing or invalid string parameter: key").dump();
    }
    std::string key = params["key"].get<std::string>();
    if (key.empty() || key.size() > 256) {
        return make_error("key must be 1-256 characters long").dump();
    }
    if (contains_dangerous_chars(key)) {
        return make_error("key contains illegal characters").dump();
    }

    if (!running_as_root()) {
        return make_error("root privileges required").dump();
    }

    if (!write_text_file(AUTH_KEY_FILE, key)) {
        return make_error("failed to persist authentication key").dump();
    }

    json reply = {
        {"status", "ok"},
        {"message", "Key updated"}
    };
    return reply.dump();
}

// ---------------------------------------------------------------------------
// tool_set_hash — set the module / file integrity hash.
//
// params: {"hash": "<hex hash>"} (32..128 hex characters, e.g. sha256)
// Persists the hash used for module integrity verification.
// Requires root.
// ---------------------------------------------------------------------------
std::string tool_set_hash(const json& params) {
    if (!params.contains("hash") || !params["hash"].is_string()) {
        return make_error("missing or invalid string parameter: hash").dump();
    }
    std::string hash = params["hash"].get<std::string>();
    if (!is_valid_hex_hash(hash)) {
        return make_error("invalid hash: expected 32-128 hex characters").dump();
    }

    if (!running_as_root()) {
        json e = make_error("root privileges required");
        e["hash"] = hash;
        return e.dump();
    }

    if (!write_text_file(MODULE_HASH_FILE, hash)) {
        return make_error("failed to persist hash").dump();
    }

    json reply = {
        {"status", "ok"},
        {"message", "hash updated"},
        {"hash", hash}
    };
    return reply.dump();
}

} // namespace tools
