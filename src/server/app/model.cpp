#include "model.hpp"

#include <cmath>
#include <cstring>

namespace npps {

const char *to_string(Language v) { return v == Language::jp ? "jp" : "en"; }
bool from_string(std::string_view s, Language &out) {
    if (s == "jp") out = Language::jp;
    else if (s == "en") out = Language::en;
    else return false;
    return true;
}

namespace model {

void fail(const std::string &path, const char *message) {
    std::string location = path;
    if (!location.empty() && location.back() == '.') location.pop_back();
    throw error::ValidationError((location.empty() ? std::string("request") : location) + ": " + message);
}

void expect_object(const Json &j, const std::string &path) {
    if (!j.is_object()) fail(path, "Input should be a valid dictionary or object");
}

const Json *field(const Json &j, std::initializer_list<const char *> keys) {
    for (const char *k : keys)
        if (const Json *v = j.find(k)) return v;
    return nullptr;
}

void load_extra(const Json &j, Json &extra, std::initializer_list<const char *> known) {
    extra = Json::object();
    for (auto &m : j.members()) {
        bool is_known = false;
        for (const char *k : known)
            if (m.first == k) {
                is_known = true;
                break;
            }
        if (!is_known) extra.members().push_back(m);
    }
}

void load(const Json &j, Json &out, const std::string &) { out = j; }

static bool parse_integer_text(std::string_view s, int64_t &out) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.remove_prefix(1);
    while (!s.empty() && isspace((unsigned char)s.back())) s.remove_suffix(1);
    std::string text(s);
    if (text.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long long v = strtoll(text.c_str(), &end, 10);
    if (errno == 0 && *end == 0) {
        out = v;
        return true;
    }
    // "1.0" is accepted when it has no fractional part.
    double d = strtod(text.c_str(), &end);
    if (*end == 0 && std::isfinite(d) && d == std::floor(d)) {
        out = (int64_t)d;
        return true;
    }
    return false;
}

void load(const Json &j, int64_t &out, const std::string &path) {
    switch (j.type()) {
    case Json::Int: out = j.as_int(); return;
    case Json::Bool: out = j.as_bool(); return;
    case Json::Double: {
        double d = j.as_double();
        if (!std::isfinite(d) || d != std::floor(d)) fail(path, "Input should be a valid integer, got a number with a fractional part");
        out = (int64_t)d;
        return;
    }
    case Json::String:
        if (parse_integer_text(j.as_string(), out)) return;
        fail(path, "Input should be a valid integer, unable to parse string as an integer");
    default: fail(path, "Input should be a valid integer");
    }
}

void load(const Json &j, double &out, const std::string &path) {
    switch (j.type()) {
    case Json::Int: out = (double)j.as_int(); return;
    case Json::Bool: out = j.as_bool(); return;
    case Json::Double: out = j.as_double(); return;
    case Json::String: {
        std::string text = j.as_string();
        char *end = nullptr;
        out = strtod(text.c_str(), &end);
        if (!text.empty() && *end == 0) return;
        fail(path, "Input should be a valid number, unable to parse string as a number");
    }
    default: fail(path, "Input should be a valid number");
    }
}

void load(const Json &j, bool &out, const std::string &path) {
    switch (j.type()) {
    case Json::Bool: out = j.as_bool(); return;
    case Json::Int:
        if (j.as_int() == 0 || j.as_int() == 1) {
            out = j.as_int();
            return;
        }
        break;
    case Json::Double:
        if (j.as_double() == 0 || j.as_double() == 1) {
            out = j.as_double() != 0;
            return;
        }
        break;
    case Json::String: {
        std::string s = j.as_string();
        for (char &c : s) c = (char)tolower((unsigned char)c);
        for (const char *t : {"1", "on", "t", "true", "y", "yes"})
            if (s == t) {
                out = true;
                return;
            }
        for (const char *f : {"0", "off", "f", "false", "n", "no"})
            if (s == f) {
                out = false;
                return;
            }
        break;
    }
    default: break;
    }
    fail(path, "Input should be a valid boolean");
}

void load(const Json &j, std::string &out, const std::string &path) {
    if (!j.is_string()) fail(path, "Input should be a valid string");
    out = j.as_string();
}

}  // namespace model
}  // namespace npps
