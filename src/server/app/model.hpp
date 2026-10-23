#pragma once
// Runtime for the generated pydantic model structs (app/models.hpp).
//
// Every model derives from model::Model. to_json() reproduces
// model_dump(): fields in declaration order, then extra="allow" fields,
// then computed fields. from_json() is model_validate() in lax mode.
//
// Models that have subclasses are held through Ptr<T> (shared_ptr) so a
// UnitItem stored in a list of Item keeps its fields, as in Python. A field
// annotated SerializeAsAny or as a union dumps the object's dynamic type;
// any other field dumps only the declared type's fields (dump_as).
#include "app/error.hpp"
#include "core/json.hpp"
#include "core/util.hpp"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace npps {

enum class Language { en, jp };
enum class PlatformType { iOS = 1, Android = 2 };
enum class XMCVerifyMode { NONE = 0, SHARED = 1, CROSS = 2 };

const char *to_string(Language v);
bool from_string(std::string_view s, Language &out);
inline Json to_json_value(Language v) { return Json(to_string(v)); }

namespace model {

struct Model {
    virtual ~Model() = default;
    virtual Json to_json() const = 0;
};

template <typename T> using Ptr = std::shared_ptr<T>;

template <typename T, typename... A> Ptr<T> make(A &&...args) { return std::make_shared<T>(std::forward<A>(args)...); }

// ---------------------------------------------------------------- dumping
inline Json dump(const Json &v) { return v; }
inline Json dump(int64_t v) { return Json(v); }
inline Json dump(int v) { return Json(v); }
inline Json dump(bool v) { return Json(v); }
inline Json dump(double v) { return Json(v); }
inline Json dump(const std::string &v) { return Json(v); }
inline Json dump(const char *v) { return Json(v); }
template <typename T> Json dump(const T &v);
template <typename T> Json dump(const std::optional<T> &v);
template <typename T> Json dump(const Ptr<T> &v);
template <typename T> Json dump(const std::vector<T> &v);

template <typename T> Json dump(const std::optional<T> &v) { return v ? dump(*v) : Json(); }
template <typename T> Json dump(const Ptr<T> &v) { return v ? v->to_json() : Json(); }
template <typename T> Json dump(const std::vector<T> &v) {
    Json out = Json::array();
    out.reserve(v.size());
    for (const auto &e : v) out.push_back(dump(e));
    return out;
}
template <typename T> Json dump(const T &v) {
    if constexpr (std::is_enum_v<T>) {
        if constexpr (requires { to_json_value(v); }) return to_json_value(v);
        else return Json((int64_t)v);
    } else {
        return v.to_json();
    }
}

// Dumps through the declared type T (pydantic without SerializeAsAny).
template <typename T, typename U> Json dump_as(const Ptr<U> &v) { return v ? v->T::to_json() : Json(); }
template <typename T, typename U> Json dump_list_as(const std::vector<Ptr<U>> &v) {
    Json out = Json::array();
    out.reserve(v.size());
    for (const auto &e : v) out.push_back(dump_as<T>(e));
    return out;
}

// ---------------------------------------------------------------- loading
[[noreturn]] void fail(const std::string &path, const char *message);
inline void missing(const std::string &path) { fail(path, "Field required"); }
void expect_object(const Json &j, const std::string &path);
const Json *field(const Json &j, std::initializer_list<const char *> keys);
void load_extra(const Json &j, Json &extra, std::initializer_list<const char *> known);

void load(const Json &j, Json &out, const std::string &path);
void load(const Json &j, int64_t &out, const std::string &path);
void load(const Json &j, bool &out, const std::string &path);
void load(const Json &j, double &out, const std::string &path);
void load(const Json &j, std::string &out, const std::string &path);

template <typename T> void load(const Json &j, T &out, const std::string &path);
template <typename T> void load(const Json &j, std::optional<T> &out, const std::string &path);
template <typename T> void load(const Json &j, Ptr<T> &out, const std::string &path);
template <typename T> void load(const Json &j, std::vector<T> &out, const std::string &path);

template <typename T> void load(const Json &j, std::optional<T> &out, const std::string &path) {
    if (j.is_null()) out.reset();
    else {
        T v{};
        load(j, v, path);
        out = std::move(v);
    }
}
template <typename T> void load(const Json &j, Ptr<T> &out, const std::string &path) {
    if (j.is_null()) out.reset();
    else {
        auto v = std::make_shared<T>();
        T::load(j, *v, path);
        out = std::move(v);
    }
}
template <typename T> void load(const Json &j, std::vector<T> &out, const std::string &path) {
    if (!j.is_array()) fail(path, "Input should be a valid list");
    out.clear();
    out.reserve(j.size());
    size_t i = 0;
    for (const Json &e : j.elements()) {
        T v{};
        load(e, v, path + std::to_string(i++) + ".");
        out.push_back(std::move(v));
    }
}
template <typename T> void load(const Json &j, T &out, const std::string &path) {
    if constexpr (std::is_enum_v<T>) {
        if constexpr (requires { from_string(std::string_view(), out); }) {
            if (!j.is_string() || !from_string(j.as_string(), out)) fail(path, "Input should be a valid enumeration member");
        } else {
            int64_t v;
            load(j, v, path);
            out = (T)v;
        }
    } else {
        T::load(j, out, path);
    }
}

// Parses request_data into a model; throws error::ValidationError.
template <typename T> T parse(const Json &j) {
    T out{};
    load(j, out, "");
    return out;
}

}  // namespace model
}  // namespace npps
