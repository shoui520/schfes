#pragma once
// npps4/idol/core.py: endpoint registration, /main.php dispatch, the /api
// batch endpoint and response signing.
//
//   static login::AuthkeyResponse login_authkey(Context &context, const login::AuthkeyRequest &request) {...}
//   NPPS_ENDPOINT("login", "authkey", Params, login_authkey,
//                 .check_version = false, .batchable = false, .xmc_verify = XMCVerifyMode::NONE);
//
// Handlers take (Context &) or (Context &, const Request &) where Request is
// a generated model, and return a model, a std::vector of models, Json or
// void (None). Throw error::IdolError for game errors and
// error::HttpError for fastapi.HTTPException.
#include "app/context.hpp"
#include "app/error.hpp"
#include "app/models.hpp"

#include <functional>
#include <string>
#include <type_traits>

namespace npps::idol {

struct EndpointOptions {
    bool check_version = true;
    bool batchable = true;
    XMCVerifyMode xmc_verify = XMCVerifyMode::SHARED;
    bool exclude_none = false;
};

struct Endpoint {
    std::string module, action;
    Context::Kind kind;
    EndpointOptions options;
    // Validates request_data into the request model (absent when the
    // endpoint takes none); throws error::ValidationError.
    std::function<std::shared_ptr<void>(const Json &request)> parse;
    // Runs the endpoint with the parsed request; returns the response as JSON.
    std::function<Json(Context &, const void *request)> invoke;
};

bool add_endpoint(Endpoint endpoint);
const Endpoint *find_endpoint(std::string_view module, std::string_view action);

template <typename R> Json to_response_json(R &&value) {
    using T = std::decay_t<R>;
    if constexpr (std::is_same_v<T, Json>) return std::forward<R>(value);
    else return model::dump(value);
}

template <typename R> bool register_endpoint(const char *module, const char *action, Context::Kind kind,
                                             R (*fn)(Context &), EndpointOptions options) {
    return add_endpoint({module, action, kind, options, nullptr, [fn](Context &c, const void *) -> Json {
                             if constexpr (std::is_void_v<R>) {
                                 fn(c);
                                 return Json();
                             } else
                                 return to_response_json(fn(c));
                         }});
}

template <typename R, typename Q> bool register_endpoint(const char *module, const char *action, Context::Kind kind,
                                                         R (*fn)(Context &, const Q &), EndpointOptions options) {
    return add_endpoint({module, action, kind, options,
                         [](const Json &request) -> std::shared_ptr<void> {
                             return std::make_shared<Q>(model::parse<Q>(request));
                         },
                         [fn](Context &c, const void *request) -> Json {
                             const Q &parsed = *static_cast<const Q *>(request);
                             if constexpr (std::is_void_v<R>) {
                                 fn(c, parsed);
                                 return Json();
                             } else
                                 return to_response_json(fn(c, parsed));
                         }});
}

#define NPPS_CONCAT_(a, b) a##b
#define NPPS_CONCAT(a, b) NPPS_CONCAT_(a, b)
#define NPPS_ENDPOINT(module, action, kind, fn, ...)                                                                \
    [[maybe_unused]] static const bool NPPS_CONCAT(npps_endpoint_, __LINE__) =                                     \
        ::npps::idol::register_endpoint(module, action, ::npps::Context::kind, fn,                                  \
                                        ::npps::idol::EndpointOptions{__VA_ARGS__})

// HTTP entry point for /main.php/... and /api.
void handle_main(const http::Request &request, http::Response &response);

// Release keys sent in release_info (the "none" download backend has none).
Json release_info();

}  // namespace npps::idol
