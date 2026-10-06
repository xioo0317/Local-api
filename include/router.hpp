// SPDX-License-Identifier: GPL-3.0-or-later
//
// router.hpp — Request dispatcher / network layer.
//
// Owns the HTTP server: network interface binding, routes,
// action parsing and dispatch to tools.cpp.

#pragma once

namespace router {

// Bind the network interface, register routes and block serving.
// Called once from main().
int run();

} // namespace router
