// SPDX-License-Identifier: GPL-3.0-or-later
//
// tools.cpp — Tool function implementations.
//
// ALL tools are placeholders. The HTTP layer only dispatches by action name;
// no tool takes parameters and nothing is executed here.
//
// detector.cpp (KSU / APatch / Magisk / SusFS handshake) is a pure detection
// library — it is compiled but intentionally NOT called yet.
//
// Root note: feature tools will require root (getuid() == 0) once they are
// implemented; running_as_root() below stays as the single root check.

#include "tools.hpp"
#include "version.hpp"

#include <nlohmann/json.hpp>
#include <unistd.h>

using json = nlohmann::json;

namespace tools {

namespace {

// True when the process is running as root (kept for upcoming root tools).
bool running_as_root() {
    return getuid() == 0;
}

json placeholder(const std::string& action) {
    return json{
        {"status",  "placeholder"},
        {"action",  action},
        {"message", "not implemented — placeholder only"}
    };
}

} // namespace

// ===========================================================================
// Dispatcher — action name -> tool function (no parameters)
// ===========================================================================

std::string execute(const std::string& action) {
    if (action == "detect")  return tool_detect();
    if (action == "version") return tool_version();
    if (action == "debug")   return tool_debug();

    json err = {
        {"status", "error"},
        {"error",  "unknown action: " + action},
        {"available_actions", list_actions()}
    };
    return err.dump();
}

json list_actions() {
    return json::array({"detect", "version", "debug"});
}

std::string debug_info() {
    return tool_debug();
}

// ===========================================================================
// Tool: detect — Root detection (KSU / APatch / Magisk / SusFS) — PLACEHOLDER
// ===========================================================================

std::string tool_detect() {
    // TODO: call ksu_detector::Detector once detection is wired up.
    // detector.cpp is compiled but deliberately not invoked here yet.
    return placeholder("detect").dump();
}

// ===========================================================================
// Tool: version — Server version + API version
// ===========================================================================

std::string tool_version() {
    json j = {
        {"status",         "ok"},
        {"server_version", local_api::SERVER_VERSION},
        {"api_version",    local_api::API_VERSION},
        {"app_name",       local_api::APP_NAME}
    };
    return j.dump();
}

// ===========================================================================
// Tool: debug — Versions, registered tools, root status
// ===========================================================================

std::string tool_debug() {
    json j = {
        {"status",         "ok"},
        {"server_version", local_api::SERVER_VERSION},
        {"api_version",    local_api::API_VERSION},
        {"pid",            getpid()},
        {"is_root",        running_as_root()},
        {"tools",          list_actions()}
    };
    return j.dump();
}

} // namespace tools
