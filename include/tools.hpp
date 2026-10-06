// SPDX-License-Identifier: GPL-3.0-or-later
//
// tools.hpp — Tool function registry.
//
// Every tool takes NO parameters (protocol: {"action":"name"} only).
// All tools are placeholders — nothing is executed. Real implementations
// (root-gated) come later.
//
// New tools: add a placeholder function + register it in execute().

#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace tools {

// Dispatch by action name. No parameters are passed to tools.
std::string execute(const std::string& action);

// List all registered action names.
nlohmann::json list_actions();

// Debug info: server version, registered tools, root status.
std::string debug_info();

// --- Tool handlers (all placeholder, no params) ---

// Root detection (KSU / APatch / Magisk / SusFS handshake) — placeholder,
// detector.cpp stays a pure library and is NOT called yet.
std::string tool_detect();

// Server version + API version info.
std::string tool_version();

// Debug / dry-run: versions, registered tools, root status.
std::string tool_debug();

} // namespace tools
