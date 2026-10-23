#include "idol.hpp"

#include "app/config.hpp"
#include "app/session.hpp"
#include "core/crypto.hpp"

#include <zlib.h>

#include <map>
#include <memory>

namespace npps::idol {

namespace {

std::map<std::pair<std::string, std::string>, Endpoint> &registry() {
    static std::map<std::pair<std::string, std::string>, Endpoint> endpoints;
    return endpoints;
}

// A response that bypasses build_response (fastapi.responses.JSONResponse).
struct PlainResponse {
    int status;
    Json body;
    std::vector<std::pair<std::string, std::string>> headers;
};

void send_plain(http::Response &res, const PlainResponse &plain) {
    res.status = plain.status;
    res.content_type = "application/json";
    res.body = plain.body.dump();
    res.headers = plain.headers;
}

Json detail(const std::string &text) { return Json::obj({{"detail", text}}); }

std::string gzip(const std::string &data) {
    z_stream z{};
    if (deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("deflateInit2 failed");
    std::string out;
    out.resize(deflateBound(&z, data.size()) + 32);
    z.next_in = (Bytef *)data.data();
    z.avail_in = (uInt)data.size();
    z.next_out = (Bytef *)out.data();
    z.avail_out = (uInt)out.size();
    int rc = deflate(&z, Z_FINISH);
    out.resize(z.total_out);
    deflateEnd(&z);
    if (rc != Z_STREAM_END) throw std::runtime_error("deflate failed");
    return out;
}

// assemble_response_data()
struct Assembled {
    Json data;
    int status_code, http_code;
};

Json without_none(const Json &v) {
    if (v.is_object()) {
        Json out = Json::object();
        for (auto &m : v.members())
            if (!m.second.is_null()) out.members().emplace_back(m.first, without_none(m.second));
        return out;
    }
    if (v.is_array()) {
        Json out = Json::array();
        for (auto &e : v.elements()) out.push_back(without_none(e));
        return out;
    }
    return v;
}

Assembled assemble(const Json &result, bool exclude_none) {
    if (result.is_null()) return {Json::array(), 200, 200};
    return {exclude_none ? without_none(result) : result, 200, 200};
}

Assembled assemble(const error::IdolError &e) {
    return {Json::obj({{"error_code", e.error_code}, {"detail", e.detail ? Json(*e.detail) : Json()}}), e.status_code,
            e.http_code};
}

Assembled assemble(const std::exception &e, const char *type_name) {
    return {Json::obj({{"error_code", error::ERROR_CODE_LIB_ERROR},
                       {"detail", std::string(type_name) + ": " + e.what()}}),
            500, 500};
}

const char *exception_name(const std::exception &e) {
    if (dynamic_cast<const error::ValidationError *>(&e)) return "ValidationError";
    if (dynamic_cast<const db::DbError *>(&e)) return "OperationalError";
    if (dynamic_cast<const JsonError *>(&e)) return "JSONDecodeError";
    if (dynamic_cast<const std::out_of_range *>(&e)) return "KeyError";
    if (dynamic_cast<const std::invalid_argument *>(&e)) return "ValueError";
    return "Exception";
}

// build_response()
void build_response(const Context &context, const Assembled &a, http::Response &res) {
    std::string body;
    body.reserve(256);
    body += "{\"response_data\": ";
    a.data.dump_to(body);
    body += ", \"release_info\": ";
    release_info().dump_to(body);
    body += ", \"status_code\": ";
    body += std::to_string(a.status_code);
    body += "}";
    res.status = a.http_code;
    res.content_type = "application/json";
    res.headers.clear();
    res.headers.emplace_back("Server-Version", util::sif_version_string(config::get_latest_version()));
    res.headers.emplace_back("X-Message-Sign",
                             crypto::sign_message(body, context.x_message_code ? &*context.x_message_code : nullptr));
    res.headers.emplace_back("status_code", std::to_string(a.status_code));
    const std::string *accept = context.request ? context.request->header("accept-encoding") : nullptr;
    std::string encoding = accept ? *accept : "identity";
    for (char &c : encoding) c = (char)tolower((unsigned char)c);
    if (encoding.find("gzip") != std::string::npos && body.size() >= 65536) {
        body = gzip(body);
        res.headers.emplace_back("Content-Encoding", "gzip");
    }
    res.body = std::move(body);
}

// SchoolIdolParams.__init__: header parsing. Throws HttpError(422).
void init_context(Context &context, const http::Request &req, Context::Kind kind) {
    context.kind = kind;
    context.request = &req;
    const std::string *authorize = req.header("authorize");
    const std::string *client_version = req.header("client-version");
    const std::string *lang = req.header("lang");
    const std::string *platform = req.header("platform-type");
    if (!authorize || !client_version || !lang || !platform) throw error::HttpError(422, "Field required");
    if (!from_string(*lang, context.lang)) throw error::HttpError(422, "Input should be 'en' or 'jp'");
    if (*platform == "1") context.platform = PlatformType::iOS;
    else if (*platform == "2") context.platform = PlatformType::Android;
    else throw error::HttpError(422, "Input should be 1 or 2");

    std::map<std::string, std::string> auth;
    for (auto &kv : util::parse_qsl(*authorize)) auth[kv.first] = kv.second;
    auto consumer = auth.find("consumerKey");
    if (consumer == auth.end() || consumer->second != config::get_consumer_key())
        throw error::HttpError(422, "Invalid consumerKey");

    size_t dot = client_version->find('.');
    try {
        if (dot == std::string::npos) throw std::invalid_argument("not enough values to unpack");
        size_t used;
        int major = std::stoi(client_version->substr(0, dot), &used);
        int minor = std::stoi(client_version->substr(dot + 1));
        context.client_version = {major, minor};
    } catch (const std::exception &) {
        throw error::HttpError(422, "invalid literal for int() with base 10: '" + *client_version + "'");
    }
    auto number = [&](const char *key, int64_t fallback) -> int64_t {
        auto it = auth.find(key);
        if (it == auth.end()) return fallback;
        try {
            size_t used;
            int64_t v = std::stoll(it->second, &used);
            return used == it->second.size() ? v : fallback;
        } catch (...) {
            return fallback;
        }
    };
    context.nonce = number("nonce", 0);
    context.timestamp = number("timeStamp", util::time());
    if (auto t = auth.find("token"); t != auth.end()) context.token_text = t->second;
    if (const std::string *xmc = req.header("x-message-code")) context.x_message_code = *xmc;
    context.raw_request_data = req.form("request_data").value_or("");
}

// SchoolIdolAuthParams/SchoolIdolUserParams.finalize(), in its own transaction.
void finalize(Context &context) {
    if (context.kind < Context::Auth) return;
    try {
        if (context.token_text) context.token = session::decapsulate_token(context, *context.token_text);
        context.commit();
    } catch (...) {
        context.rollback();
        throw;
    }
    if (!context.token) throw error::HttpError(403, "Invalid token");
    if (context.kind == Context::User && context.token->user_id == 0) throw error::HttpError(403, "Not logged in!");
}

// client_check(). Returns true when a response was produced.
bool client_check(Context &context, bool check_version, XMCVerifyMode mode, http::Response &res) {
    if (config::is_maintenance()) {
        send_plain(res, {200, Json::array(), {{"Maintenance", "1"}}});
        return true;
    }
    if (mode != XMCVerifyMode::NONE && !context.token) throw error::HttpError(403, "Invalid token");
    if (config::need_xmc_verify() && mode != XMCVerifyMode::NONE) {
        if (!context.x_message_code) throw error::HttpError(422, "Invalid X-Message-Code");
        if (context.kind < Context::Auth) throw error::HttpError(422, "Invalid X-Message-Code (no token)");
        Bytes key;
        if (mode == XMCVerifyMode::SHARED) key = util::xorbytes(context.token->client_key, context.token->server_key);
        else {
            const std::string &base = config::get_base_xorpad(), &app = config::get_application_key();
            key = util::xorbytes(std::string_view(base).substr(16), std::string_view(app).substr(0, 16)) +
                  util::xorbytes(std::string_view(base).substr(0, 16), std::string_view(app).substr(16));
        }
        std::string xmc = util::hex(crypto::hmac_sha1(context.raw_request_data, key), true);
        std::string given = *context.x_message_code;
        for (char &c : given) c = (char)toupper((unsigned char)c);
        if (xmc != given) throw error::HttpError(422, "X-Message-Code does not match");
    }
    if (check_version && config::get_latest_version() != context.client_version) {
        build_response(context, assemble(Json(), false), res);
        return true;
    }
    return false;
}

Json parse_request_data(const Context &context) {
    // _get_request_data() declares the X-Message-Code header without a
    // default, so FastAPI requires it whenever there is request data.
    if (!context.x_message_code) throw error::HttpError(422, "X-Message-Code: Field required");
    if (!context.request->form("request_data")) throw error::HttpError(422, "request_data: Field required");
    try {
        return Json::parse(context.raw_request_data);
    } catch (const JsonError &e) {
        throw error::HttpError(422, std::string("request_data: Invalid JSON: ") + e.what());
    }
}

void schedule_tasks(Context &context, http::Response &res) {
    for (auto &task : context.take_tasks()) res.after_send.push_back(std::move(task));
}

void run_endpoint(const Endpoint &ep, const http::Request &req, http::Response &res) {
    Context context;
    init_context(context, req, ep.kind);
    // FastAPI validates the request model before running the endpoint.
    std::shared_ptr<void> request;
    if (ep.parse) {
        Json data = parse_request_data(context);
        try {
            request = ep.parse(data);
        } catch (const error::ValidationError &e) {
            throw error::HttpError(422, e.what());
        }
    }
    finalize(context);
    if (client_check(context, ep.options.check_version, ep.options.xmc_verify, res)) return;
    try {
        Json result = ep.invoke(context, request.get());
        context.commit();
        build_response(context, assemble(result, ep.options.exclude_none), res);
    } catch (const error::IdolError &e) {
        context.rollback();
        build_response(context, assemble(e), res);
    } catch (...) {
        context.rollback();
        throw;
    }
    schedule_tasks(context, res);
}

struct BatchItem {
    std::string module, action;
};

void run_batch(const http::Request &req, http::Response &res) {
    Context context;
    init_context(context, req, Context::User);
    Json items = parse_request_data(context);
    if (!items.is_array()) throw error::HttpError(422, "request_data: Input should be a valid list");
    for (const Json &item : items.elements()) {
        const Json *m = item.find("module"), *a = item.find("action");
        if (!item.is_object() || !m || !a || !m->is_string() || !a->is_string())
            throw error::HttpError(422, "request_data: module and action are required");
    }
    finalize(context);
    if (client_check(context, true, XMCVerifyMode::SHARED, res)) return;

    Json response = Json::array();
    try {
        for (const Json &item : items.elements()) {
            const std::string &module = item.at("module").as_string(), &action = item.at("action").as_string();
            Assembled a;
            try {
                const Endpoint *ep = find_endpoint(module, action);
                if (!ep || !ep->options.batchable || ep->options.xmc_verify == XMCVerifyMode::CROSS) {
                    std::string message = "Endpoint not found: " + module + "/" + action;
                    util::log("%s %s", message.c_str(), item.dump().c_str());
                    throw error::IdolError(error::ERROR_CODE_LIB_ERROR, 404, message, 404);
                }
                std::shared_ptr<void> request = ep->parse ? ep->parse(item) : nullptr;
                a = assemble(ep->invoke(context, request.get()), ep->options.exclude_none);
            } catch (const error::IdolError &e) {
                a = assemble(e);
            } catch (const std::exception &e) {
                util::log("Error processing \"%s/%s\": %s", module.c_str(), action.c_str(), e.what());
                a = assemble(e, exception_name(e));
            }
            response.push_back(Json::obj({{"result", std::move(a.data)},
                                          {"status", a.status_code},
                                          {"commandNum", false},
                                          {"timeStamp", util::time()}}));
        }
        context.commit();
    } catch (...) {
        context.rollback();
        throw;
    }
    build_response(context, assemble(response, false), res);
    schedule_tasks(context, res);
}

}  // namespace

bool add_endpoint(Endpoint endpoint) {
    auto key = std::make_pair(endpoint.module, endpoint.action);
    if (registry().count(key)) throw std::logic_error("Endpoint " + endpoint.module + "/" + endpoint.action +
                                                      " is already registered!");
    registry().emplace(key, std::move(endpoint));
    return true;
}

const Endpoint *find_endpoint(std::string_view module, std::string_view action) {
    auto it = registry().find(std::make_pair(std::string(module), std::string(action)));
    return it == registry().end() ? nullptr : &it->second;
}

Json release_info() { return Json::array(); }

void handle_main(const http::Request &req, http::Response &res) {
    std::string_view path = req.path;
    try {
        if (path == "/main.php/api") {
            if (req.method != "POST") throw error::HttpError(405, "Method Not Allowed");
            run_batch(req, res);
            return;
        }
        const Endpoint *ep = nullptr;
        if (util::starts_with(path, "/main.php/")) {
            std::string_view rest = path.substr(10);
            size_t slash = rest.find('/');
            if (slash != std::string_view::npos && rest.find('/', slash + 1) == std::string_view::npos)
                ep = find_endpoint(rest.substr(0, slash), rest.substr(slash + 1));
        }
        if (!ep || (ep->parse ? req.method != "POST" : (req.method != "POST" && req.method != "GET"))) {
            const std::string *host = req.header("host");
            send_plain(res, {404, detail("Endpoint not found: http://" + (host ? *host : std::string("localhost")) +
                                         req.target),
                             {{"Maintenance", "1"}}});
            return;
        }
        run_endpoint(*ep, req, res);
    } catch (const error::HttpError &e) {
        send_plain(res, {e.status, detail(e.detail), {}});
    } catch (const std::exception &e) {
        util::log("Unhandled exception on %s: %s", req.path.c_str(), e.what());
        send_plain(res, {500,
                         Json::obj({{"exception", exception_name(e)}, {"message", e.what()}, {"traceback", Json::array()}}),
                         {{"Maintenance", "1"}}});
    }
}

}  // namespace npps::idol
