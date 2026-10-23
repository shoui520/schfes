#pragma once
// Minimal HTTP/1.1 server for the game client: keep-alive, Content-Length
// bodies, Expect: 100-continue, form-urlencoded and multipart forms. One
// thread serves every connection; requests are handled one at a time.
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace npps::http {

struct Request {
    std::string method, target, path, query;
    std::vector<std::pair<std::string, std::string>> headers;  // names lowercased
    std::string body;

    const std::string *header(std::string_view lowercase_name) const;
    // Form field from a urlencoded or multipart body.
    std::optional<std::string> form(std::string_view name) const;
    // Query string parameter.
    std::optional<std::string> param(std::string_view name) const;
};

struct Response {
    int status = 200;
    std::string content_type = "application/json";
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    // Run after the response has been sent (FastAPI background tasks).
    std::vector<std::function<void()>> after_send;
    void set(int code, std::string text, std::string type = "text/plain; charset=utf-8") {
        status = code;
        body = std::move(text);
        content_type = std::move(type);
    }
};

using Handler = std::function<void(const Request &, Response &)>;

class Server {
public:
    // Listens on address:port (IPv4). Throws std::runtime_error on failure.
    Server(const char *address, int port);
    ~Server();
    Server(const Server &) = delete;
    int port() const { return port_; }
    // Serves until stop() is called from another thread.
    void run(const Handler &handler);
    void stop();

private:
    struct Connection;
    bool service(Connection &c, const Handler &handler);
    int listener_ = -1, port_ = 0;
    volatile bool running_ = false;
};

const char *status_text(int status);

}  // namespace npps::http
