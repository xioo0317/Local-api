// SPDX-License-Identifier: GPL-3.0-or-later
//
// tools.hpp — Tool function registry.
//
// Each tool is a function that takes optional params and returns JSON.
// tools::execute() dispatches by action name.
// New tools: add a handler function + register in execute().

#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace tools {

// Dispatch by action name. params is the full parsed JSON body.
std::string execute(const std::string& action, const nlohmann::json& params);

// List all registered action names.
nlohmann::json list_actions();

// Debug info: all tools, versions, dry-run output.
std::string debug_info();

// --- Individual tool handlers ---

// Root detection (KernelSU / APatch / Magisk / SusFS handshake)
std::string tool_detect(const nlohmann::json& params);

// Server version + API version
std::string tool_version(const nlohmann::json& params);

// Debug / dry-run: tool status, versions, environment info
std::string tool_debug(const nlohmann::json& params);

// --- Placeholder tools (implement when needed) ---

// System information (device model, Android version, kernel, etc.)
std::string tool_sysinfo(const nlohmann::json& params);

// Module management (list installed modules, enable/disable)
std::string tool_modules(const nlohmann::json& params);

// Configuration read/write
std::string tool_config(const nlohmann::json& params);

// --- New feature tools (all require root privileges) ---

// Hide / restore the coverRoot launcher icon (pm disable / pm enable)
std::string tool_hide_icon(const nlohmann::json& params);

// Configure SusFS hidden paths in one shot
std::string tool_susfs_setup(const nlohmann::json& params);

// Configure the list of apps from which root / the manager are hidden
std::string tool_hide_app_list(const nlohmann::json& params);

// Update the coverRoot authentication key
std::string tool_update_key(const nlohmann::json& params);

// Set the module / file hash used for integrity checks
std::string tool_set_hash(const nlohmann::json& params);

} // namespace tools
