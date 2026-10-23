#include "server.hpp"

#include "app/databases.hpp"
#include "app/idol.hpp"
#include "data/data.hpp"

#include <map>

namespace npps::app {

static std::map<std::pair<std::string, std::string>, RouteHandler> &routes() {
    static std::map<std::pair<std::string, std::string>, RouteHandler> table;
    return table;
}

bool add_route(std::string method, std::string path, RouteHandler handler) {
    routes()[{std::move(method), std::move(path)}] = std::move(handler);
    return true;
}

void initialize() {
    open_databases();
    // Parse server_data.json now rather than in the first request that needs it.
    data::get();
}

void shutdown() { close_databases(); }

void handle_request(const http::Request &req, http::Response &res) {
    uint64_t start = util::monotonic_us();
    if (util::starts_with(req.path, "/main.php/")) idol::handle_main(req, res);
    else {
        auto it = routes().find({req.method == "HEAD" ? "GET" : req.method, req.path});
        if (it != routes().end()) it->second(req, res);
        else {
            res.status = 404;
            res.body = Json::obj({{"detail", "Not Found"}}).dump();
        }
    }
    util::log("%s %s -> %d (%.1f ms)", req.method.c_str(), req.path.c_str(), res.status,
              (util::monotonic_us() - start) / 1000.0);
}

}  // namespace npps::app
