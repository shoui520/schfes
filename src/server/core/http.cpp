#include "http.hpp"

#include "util.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace npps::http {

static constexpr size_t MAX_HEADER = 64 * 1024;
static constexpr size_t MAX_BODY = 16 * 1024 * 1024;
static constexpr size_t MAX_CONNECTIONS = 16;
static constexpr int IDLE_TIMEOUT_MS = 120000;

const char *status_text(int status) {
    switch (status) {
    case 100: return "Continue";
    case 200: return "OK";
    case 204: return "No Content";
    case 302: return "Found";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    case 422: return "Unprocessable Entity";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "Unknown";
    }
}

static std::string lower(std::string_view s) {
    std::string out(s);
    for (char &c : out)
        if (c >= 'A' && c <= 'Z') c += 32;
    return out;
}

static std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

const std::string *Request::header(std::string_view name) const {
    for (auto &h : headers)
        if (h.first == name) return &h.second;
    return nullptr;
}

std::optional<std::string> Request::param(std::string_view name) const {
    for (auto &p : util::parse_qsl(query))
        if (p.first == name) return p.second;
    return std::nullopt;
}

// Parameter of a header value such as `multipart/form-data; boundary=x`.
static std::optional<std::string> header_param(std::string_view value, std::string_view key) {
    size_t at = 0;
    while ((at = value.find(';', at)) != std::string_view::npos) {
        std::string_view part = trim(value.substr(at + 1));
        size_t end = part.find(';');
        if (end != std::string_view::npos) part = part.substr(0, end);
        size_t eq = part.find('=');
        if (eq != std::string_view::npos && lower(trim(part.substr(0, eq))) == key) {
            std::string_view v = trim(part.substr(eq + 1));
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
            return std::string(v);
        }
        at++;
    }
    return std::nullopt;
}

std::optional<std::string> Request::form(std::string_view name) const {
    const std::string *type = header("content-type");
    std::string mime = type ? lower(trim(std::string_view(*type).substr(0, type->find(';')))) : "";
    if (mime == "multipart/form-data") {
        auto boundary = header_param(*type, "boundary");
        if (!boundary) return std::nullopt;
        std::string delimiter = "--" + *boundary;
        size_t at = body.find(delimiter);
        while (at != std::string::npos) {
            at += delimiter.size();
            if (body.compare(at, 2, "--") == 0) break;
            size_t headers_end = body.find("\r\n\r\n", at);
            if (headers_end == std::string::npos) break;
            std::string_view part_headers(body.data() + at, headers_end - at);
            size_t content = headers_end + 4;
            size_t next = body.find("\r\n" + delimiter, content);
            if (next == std::string::npos) break;
            for (const std::string &line : util::split(part_headers, '\n')) {
                size_t colon = line.find(':');
                if (colon == std::string::npos || lower(trim(std::string_view(line).substr(0, colon))) != "content-disposition")
                    continue;
                auto field = header_param(std::string_view(line).substr(colon + 1), "name");
                if (field && *field == name) return body.substr(content, next - content);
            }
            at = next + 2;
        }
        return std::nullopt;
    }
    for (auto &p : util::parse_qsl(body))
        if (p.first == name) return p.second;
    return std::nullopt;
}

struct Server::Connection {
    int fd = -1;
    std::string in;
    uint64_t last_us = 0;
    bool continue_sent = false;
    ~Connection() {
        if (fd >= 0) close(fd);
    }
};

Server::Server(const char *address, int port) {
    listener_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_ < 0) throw std::runtime_error("socket failed");
    int one = 1;
    setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    inet_pton(AF_INET, address, &sa.sin_addr);
    if (bind(listener_, (sockaddr *)&sa, sizeof(sa)) < 0 || listen(listener_, 8) < 0) {
        int e = errno;
        close(listener_);
        throw std::runtime_error(std::string("cannot listen: ") + strerror(e));
    }
    socklen_t len = sizeof(sa);
    getsockname(listener_, (sockaddr *)&sa, &len);
    port_ = ntohs(sa.sin_port);
}

Server::~Server() {
    if (listener_ >= 0) close(listener_);
}

void Server::stop() { running_ = false; }

static bool send_all(int fd, const char *data, size_t n) {
    while (n) {
        ssize_t r = send(fd, data, n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        data += r;
        n -= (size_t)r;
    }
    return true;
}

// Returns false when the connection should be closed.
bool Server::service(Connection &c, const Handler &handler) {
    for (;;) {
        size_t head_end = c.in.find("\r\n\r\n");
        if (head_end == std::string::npos) return c.in.size() <= MAX_HEADER;
        Request req;
        std::string_view head(c.in.data(), head_end);
        size_t line_end = head.find("\r\n");
        std::string_view line = head.substr(0, line_end);
        size_t s1 = line.find(' '), s2 = line.rfind(' ');
        if (s1 == std::string_view::npos || s2 <= s1) return false;
        req.method = std::string(line.substr(0, s1));
        req.target = std::string(line.substr(s1 + 1, s2 - s1 - 1));
        bool http10 = line.substr(s2 + 1) == "HTTP/1.0";
        if (line_end != std::string_view::npos) {
            for (const std::string &h : util::split(head.substr(line_end + 2), '\n')) {
                size_t colon = h.find(':');
                if (colon == std::string::npos) continue;
                req.headers.emplace_back(lower(trim(std::string_view(h).substr(0, colon))),
                                         std::string(trim(std::string_view(h).substr(colon + 1))));
            }
        }
        size_t length = 0;
        if (const std::string *cl = req.header("content-length")) length = strtoul(cl->c_str(), nullptr, 10);
        if (const std::string *te = req.header("transfer-encoding"); te && lower(*te) != "identity") return false;
        if (length > MAX_BODY) return false;
        size_t total = head_end + 4 + length;
        if (c.in.size() < total) {
            // libcurl waits up to a second for this before sending a body.
            const std::string *expect = req.header("expect");
            if (expect && lower(*expect) == "100-continue" && !c.continue_sent) {
                static const char cont[] = "HTTP/1.1 100 Continue\r\n\r\n";
                if (!send_all(c.fd, cont, sizeof(cont) - 1)) return false;
                c.continue_sent = true;
            }
            return true;
        }
        req.body = c.in.substr(head_end + 4, length);
        c.in.erase(0, total);
        c.continue_sent = false;
        size_t q = req.target.find('?');
        req.path = util::url_decode(req.target.substr(0, q), false);
        if (q != std::string::npos) req.query = req.target.substr(q + 1);

        Response res;
        try {
            handler(req, res);
        } catch (const std::exception &e) {
            util::log("HTTP handler error on %s: %s", req.path.c_str(), e.what());
            res = Response();
            res.set(500, "Internal Server Error");
        }
        const std::string *conn = req.header("connection");
        bool keep = conn ? lower(*conn) != "close" : !http10;
        // %llu: the Vita's newlib printf has no C99 length modifiers such as %zu.
        std::string out = util::format("HTTP/1.1 %d %s\r\nContent-Length: %llu\r\nContent-Type: %s\r\nConnection: %s\r\n",
                                       res.status, status_text(res.status), (unsigned long long)res.body.size(),
                                       res.content_type.c_str(),
                                       keep ? "keep-alive" : "close");
        for (auto &h : res.headers) out += h.first + ": " + h.second + "\r\n";
        out += "\r\n";
        if (req.method != "HEAD") out += res.body;
        bool sent = send_all(c.fd, out.data(), out.size());
        for (auto &task : res.after_send) {
            try {
                task();
            } catch (const std::exception &e) {
                util::log("HTTP background task failed: %s", e.what());
            }
        }
        if (!sent || !keep) return false;
    }
}

void Server::run(const Handler &handler) {
    running_ = true;
    std::vector<std::unique_ptr<Connection>> conns;
    std::vector<pollfd> fds;
    char buf[16384];
    while (running_) {
        fds.clear();
        fds.push_back({listener_, POLLIN, 0});
        for (auto &c : conns) fds.push_back({c->fd, POLLIN, 0});
        int n = poll(fds.data(), fds.size(), 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            util::log("HTTP poll failed: %s", strerror(errno));
            continue;
        }
        uint64_t now = util::monotonic_us();
        for (size_t i = 1; i < fds.size(); i++) {
            Connection &c = *conns[i - 1];
            bool keep = true;
            if (fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                ssize_t r = recv(c.fd, buf, sizeof(buf), 0);
                if (r <= 0) keep = false;
                else {
                    c.in.append(buf, r);
                    c.last_us = now;
                    keep = service(c, handler);
                }
            } else if (now - c.last_us > (uint64_t)IDLE_TIMEOUT_MS * 1000) keep = false;
            if (!keep) {
                c.~Connection();
                new (&c) Connection();
            }
        }
        conns.erase(std::remove_if(conns.begin(), conns.end(), [](auto &c) { return c->fd < 0; }), conns.end());
        if (fds[0].revents & POLLIN) {
            int fd = accept(listener_, nullptr, nullptr);
            if (fd >= 0) {
                if (conns.size() >= MAX_CONNECTIONS) close(fd);
                else {
                    int one = 1;
                    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                    auto c = std::make_unique<Connection>();
                    c->fd = fd;
                    c->last_us = now;
                    conns.push_back(std::move(c));
                }
            }
        }
    }
}

}  // namespace npps::http
