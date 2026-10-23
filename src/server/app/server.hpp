#pragma once
// Top-level request routing (npps4/app/app.py) and server lifetime.
#include "core/http.hpp"

#include <functional>
#include <string>

namespace npps::app {

// Opens the databases and loads server data. The server key must already
// be loaded (crypto::load_server_key).
void initialize();
void shutdown();
void handle_request(const http::Request &request, http::Response &response);

// Extra routes outside /main.php (webview pages, server_info.json).
using RouteHandler = std::function<void(const http::Request &, http::Response &)>;
bool add_route(std::string method, std::string path, RouteHandler handler);

}  // namespace npps::app
