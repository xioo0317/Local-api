// SPDX-License-Identifier: GPL-3.0-or-later
//
// router.cpp — Request dispatcher implementation.
//
// Network layer: binds the interface, exposes a single endpoint
//   POST /  with {"action":"name"}  -> tools::execute("name")
// plus GET /debug for service introspection.
//
// The HTTP layer only parses and dispatches — no tool is executed here.

#include "router.hpp"
#include "tools.hpp"
#include "version.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>

using json = nlohmann::json;

namespace router {

namespace {

void handle_post(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body);
    } catch (...) {
        json err = {{"status", "error"}, {"error", "invalid JSON body"}};
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
    }

    const std::string action = body.value("action", "");
    if (action.empty()) {
        json err = {
            {"status", "error"},
            {"error",  "missing 'action' field"},
            {"available_actions", tools::list_actions()}
        };
        res.status = 400;
        res.set_content(err.dump(), "application/json");
        return;
    }

    res.set_content(tools::execute(action), "application/json");
}

void handle_debug(const httplib::Request&, httplib::Response& res) {
    res.set_content(tools::debug_info(), "application/json");
}

} // namespace

int run() {
    httplib::Server svr;

    // Single endpoint: POST / — {"action":"name"}
    svr.Post("/", handle_post);

    // GET /debug — versions, registered tools, root status
    svr.Get("/debug", handle_debug);

    // JSON 404 instead of any HTML/console noise
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.status == 404) {
            json err = {
                {"error", "not found"},
                {"hint",  "POST / with {\"action\":\"...\"}  |  GET /debug"}
            };
            res.set_content(err.dump(), "application/json");
        }
    });

    std::cout << local_api::APP_NAME << " v" << local_api::SERVER_VERSION
              << " listening on 0.0.0.0:" << local_api::SERVER_PORT << std::endl;

    return svr.listen("0.0.0.0", local_api::SERVER_PORT) ? 0 : 1;
}

} // namespace router
