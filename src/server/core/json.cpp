#include "json.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace npps {

const char *json_type_name(Json::Type type) {
    static const char *const names[] = {"null", "bool", "int", "float", "string", "array", "object"};
    return names[type];
}

static void fail(const char *what, Json::Type got) {
    throw JsonError(std::string("expected ") + what + ", got " + json_type_name(got));
}

Json::Json(const Json &other) : type_(other.type_) {
    switch (type_) {
    case String: u_.s = new std::string(*other.u_.s); break;
    case Array: u_.a = new Elements(*other.u_.a); break;
    case Object: u_.o = new Members(*other.u_.o); break;
    default: u_ = other.u_;
    }
}

Json &Json::operator=(const Json &other) {
    if (this != &other) {
        Json copy(other);
        *this = std::move(copy);
    }
    return *this;
}

Json &Json::operator=(Json &&other) noexcept {
    if (this != &other) {
        release();
        type_ = other.type_;
        u_ = other.u_;
        other.type_ = Null;
    }
    return *this;
}

void Json::release() noexcept {
    switch (type_) {
    case String: delete u_.s; break;
    case Array: delete u_.a; break;
    case Object: delete u_.o; break;
    default: break;
    }
    type_ = Null;
}

Json Json::obj(std::initializer_list<std::pair<const char *, Json>> members) {
    Members m;
    m.reserve(members.size());
    for (auto &p : members) m.emplace_back(p.first, p.second);
    return Json(std::move(m));
}

Json Json::arr(std::initializer_list<Json> elements) { return Json(Elements(elements)); }

bool Json::as_bool() const {
    if (type_ != Bool) fail("bool", type_);
    return u_.b;
}

int64_t Json::as_int() const {
    if (type_ == Int) return u_.i;
    if (type_ == Bool) return u_.b;
    if (type_ == Double && std::floor(u_.d) == u_.d && std::fabs(u_.d) < 9.2e18) return (int64_t)u_.d;
    fail("int", type_);
    return 0;
}

double Json::as_double() const {
    if (type_ == Double) return u_.d;
    if (type_ == Int) return (double)u_.i;
    fail("float", type_);
    return 0;
}

const std::string &Json::as_string() const {
    if (type_ != String) fail("string", type_);
    return *u_.s;
}

Json::Elements &Json::elements() {
    if (type_ != Array) fail("array", type_);
    return *u_.a;
}
const Json::Elements &Json::elements() const {
    if (type_ != Array) fail("array", type_);
    return *u_.a;
}
Json::Members &Json::members() {
    if (type_ != Object) fail("object", type_);
    return *u_.o;
}
const Json::Members &Json::members() const {
    if (type_ != Object) fail("object", type_);
    return *u_.o;
}

int64_t Json::to_int() const {
    if (type_ == String) {
        const std::string &s = *u_.s;
        char *end = nullptr;
        errno = 0;
        long long v = std::strtoll(s.c_str(), &end, 10);
        if (s.empty() || errno || *end) throw JsonError("expected integer string, got '" + s + "'");
        return v;
    }
    return as_int();
}

double Json::to_double() const {
    if (type_ == String) {
        const std::string &s = *u_.s;
        char *end = nullptr;
        double v = std::strtod(s.c_str(), &end);
        if (s.empty() || *end) throw JsonError("expected number string, got '" + s + "'");
        return v;
    }
    if (type_ == Bool) return u_.b;
    return as_double();
}

bool Json::to_bool() const {
    switch (type_) {
    case Bool: return u_.b;
    case Int:
        if (u_.i == 0 || u_.i == 1) return u_.i;
        break;
    case Double:
        if (u_.d == 0 || u_.d == 1) return u_.d != 0;
        break;
    case String: {
        const std::string &s = *u_.s;
        for (const char *t : {"1", "true", "yes", "on", "y", "t"})
            if (strcasecmp(s.c_str(), t) == 0) return true;
        for (const char *f : {"0", "false", "no", "off", "n", "f"})
            if (strcasecmp(s.c_str(), f) == 0) return false;
        break;
    }
    default: break;
    }
    throw JsonError(std::string("expected bool, got ") + json_type_name(type_));
}

std::string Json::to_string() const {
    if (type_ == String) return *u_.s;
    if (type_ == Int || type_ == Double) {
        std::string out;
        dump_to(out);
        return out;
    }
    fail("string", type_);
    return {};
}

size_t Json::size() const noexcept {
    switch (type_) {
    case Array: return u_.a->size();
    case Object: return u_.o->size();
    case String: return u_.s->size();
    default: return 0;
    }
}

Json &Json::operator[](std::string_view key) {
    if (type_ == Null) *this = object();
    if (type_ != Object) fail("object", type_);
    for (auto &m : *u_.o)
        if (m.first == key) return m.second;
    u_.o->emplace_back(std::string(key), Json());
    return u_.o->back().second;
}

const Json &Json::at(std::string_view key) const {
    const Json *v = find(key);
    if (!v) throw JsonError("missing field '" + std::string(key) + "'");
    return *v;
}

Json *Json::find(std::string_view key) noexcept {
    if (type_ != Object) return nullptr;
    for (auto &m : *u_.o)
        if (m.first == key) return &m.second;
    return nullptr;
}

const Json *Json::find(std::string_view key) const noexcept { return const_cast<Json *>(this)->find(key); }

bool Json::erase(std::string_view key) {
    if (type_ != Object) return false;
    for (auto it = u_.o->begin(); it != u_.o->end(); ++it)
        if (it->first == key) {
            u_.o->erase(it);
            return true;
        }
    return false;
}

int64_t Json::get_int(std::string_view key, int64_t fallback) const {
    const Json *v = find(key);
    return v && !v->is_null() ? v->to_int() : fallback;
}

std::string Json::get_string(std::string_view key, std::string_view fallback) const {
    const Json *v = find(key);
    return v && !v->is_null() ? v->to_string() : std::string(fallback);
}

bool Json::get_bool(std::string_view key, bool fallback) const {
    const Json *v = find(key);
    return v && !v->is_null() ? v->to_bool() : fallback;
}

Json &Json::operator[](size_t index) {
    Elements &e = elements();
    if (index >= e.size()) throw JsonError("array index out of range");
    return e[index];
}

const Json &Json::operator[](size_t index) const {
    const Elements &e = elements();
    if (index >= e.size()) throw JsonError("array index out of range");
    return e[index];
}

Json &Json::push_back(Json value) {
    if (type_ == Null) *this = array();
    elements().push_back(std::move(value));
    return u_.a->back();
}

void Json::reserve(size_t n) {
    if (type_ == Null) *this = array();
    if (type_ == Array) u_.a->reserve(n);
    else if (type_ == Object) u_.o->reserve(n);
}

bool Json::operator==(const Json &other) const {
    if (is_number() && other.is_number()) {
        if (type_ == Int && other.type_ == Int) return u_.i == other.u_.i;
        return as_double() == other.as_double();
    }
    if (type_ != other.type_) return false;
    switch (type_) {
    case Null: return true;
    case Bool: return u_.b == other.u_.b;
    case String: return *u_.s == *other.u_.s;
    case Array: return *u_.a == *other.u_.a;
    case Object: {
        // Python dict equality ignores order.
        if (u_.o->size() != other.u_.o->size()) return false;
        for (auto &m : *u_.o) {
            const Json *v = other.find(m.first);
            if (!v || *v != m.second) return false;
        }
        return true;
    }
    default: return false;
    }
}

// ---- Serialization ----

static const char HEX[] = "0123456789abcdef";

static void unicode_escape(std::string &out, unsigned cp) {
    char b[6] = {'\\', 'u', HEX[(cp >> 12) & 15], HEX[(cp >> 8) & 15], HEX[(cp >> 4) & 15], HEX[cp & 15]};
    out.append(b, 6);
}

void json_escape_to(std::string &out, std::string_view text) {
    out.push_back('"');
    const unsigned char *p = (const unsigned char *)text.data(), *end = p + text.size();
    while (p < end) {
        const unsigned char *run = p;
        while (p < end && *p >= 0x20 && *p < 0x7f && *p != '"' && *p != '\\') p++;
        out.append((const char *)run, p - run);
        if (p >= end) break;
        unsigned c = *p;
        switch (c) {
        case '"': out += "\\\""; p++; continue;
        case '\\': out += "\\\\"; p++; continue;
        case '\n': out += "\\n"; p++; continue;
        case '\r': out += "\\r"; p++; continue;
        case '\t': out += "\\t"; p++; continue;
        case '\b': out += "\\b"; p++; continue;
        case '\f': out += "\\f"; p++; continue;
        default: break;
        }
        if (c < 0x80) {
            unicode_escape(out, c);
            p++;
            continue;
        }
        // Decode UTF-8; invalid sequences become U+FFFD.
        unsigned cp = 0xfffd, need = 0;
        if (c >= 0xc2 && c <= 0xdf) cp = c & 0x1f, need = 1;
        else if (c >= 0xe0 && c <= 0xef) cp = c & 0x0f, need = 2;
        else if (c >= 0xf0 && c <= 0xf4) cp = c & 0x07, need = 3;
        p++;
        if (need) {
            const unsigned char *q = p;
            unsigned v = cp;
            unsigned n = 0;
            for (; n < need && q < end && (*q & 0xc0) == 0x80; n++, q++) v = (v << 6) | (*q & 0x3f);
            bool overlong = (need == 2 && v < 0x800) || (need == 3 && v < 0x10000);
            if (n == need && !overlong && v <= 0x10ffff && !(v >= 0xd800 && v <= 0xdfff)) {
                cp = v;
                p = q;
            } else cp = 0xfffd;
        }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            unicode_escape(out, 0xd800 | (cp >> 10));
            unicode_escape(out, 0xdc00 | (cp & 0x3ff));
        } else unicode_escape(out, cp);
    }
    out.push_back('"');
}

// Python float.__repr__: shortest round-trip digits, fixed notation for
// exponents in [-4, 16), otherwise d.ddde+XX.
void json_double_to(std::string &out, double value) {
    if (std::isnan(value)) {
        out += "NaN";
        return;
    }
    if (std::isinf(value)) {
        out += value > 0 ? "Infinity" : "-Infinity";
        return;
    }
    if (value == 0) {
        out += std::signbit(value) ? "-0.0" : "0.0";
        return;
    }
    char buf[40];
    int precision = 1;
    for (; precision <= 17; precision++) {
        snprintf(buf, sizeof(buf), "%.*e", precision - 1, value);
        if (std::strtod(buf, nullptr) == value) break;
    }
    // buf: [-]d[.ddd]e[+-]XX
    const char *p = buf;
    bool negative = *p == '-';
    if (negative) p++;
    std::string digits;
    for (; *p && *p != 'e'; p++)
        if (*p != '.') digits.push_back(*p);
    int exponent = atoi(p + 1);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    if (negative) out.push_back('-');
    if (exponent >= -4 && exponent < 16) {
        if (exponent < 0) {
            out += "0.";
            out.append(-exponent - 1, '0');
            out += digits;
        } else if ((size_t)exponent + 1 >= digits.size()) {
            out += digits;
            out.append(exponent + 1 - digits.size(), '0');
            out += ".0";
        } else {
            out.append(digits, 0, exponent + 1);
            out.push_back('.');
            out.append(digits, exponent + 1, std::string::npos);
        }
    } else {
        out.push_back(digits[0]);
        if (digits.size() > 1) {
            out.push_back('.');
            out.append(digits, 1, std::string::npos);
        }
        snprintf(buf, sizeof(buf), "e%c%02d", exponent < 0 ? '-' : '+', exponent < 0 ? -exponent : exponent);
        out += buf;
    }
}

void Json::dump_to(std::string &out, bool exclude_none) const {
    switch (type_) {
    case Null: out += "null"; break;
    case Bool: out += u_.b ? "true" : "false"; break;
    case Int: {
        char b[24];
        int n = snprintf(b, sizeof(b), "%lld", (long long)u_.i);
        out.append(b, n);
        break;
    }
    case Double: json_double_to(out, u_.d); break;
    case String: json_escape_to(out, *u_.s); break;
    case Array: {
        out.push_back('[');
        bool first = true;
        for (const Json &e : *u_.a) {
            if (!first) out += ", ";
            first = false;
            e.dump_to(out, exclude_none);
        }
        out.push_back(']');
        break;
    }
    case Object: {
        out.push_back('{');
        bool first = true;
        for (const Member &m : *u_.o) {
            if (exclude_none && m.second.is_null()) continue;
            if (!first) out += ", ";
            first = false;
            json_escape_to(out, m.first);
            out += ": ";
            m.second.dump_to(out, exclude_none);
        }
        out.push_back('}');
        break;
    }
    }
}

std::string Json::dump(bool exclude_none) const {
    std::string out;
    dump_to(out, exclude_none);
    return out;
}

// ---- Parsing ----

namespace {
struct Parser {
    const char *p, *end;
    unsigned depth = 0;

    [[noreturn]] void error(const char *what) {
        throw JsonError(std::string("JSON parse error: ") + what);
    }
    void ws() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    }
    bool literal(const char *word) {
        size_t n = strlen(word);
        if ((size_t)(end - p) >= n && !memcmp(p, word, n)) {
            p += n;
            return true;
        }
        return false;
    }
    unsigned hex4() {
        if (end - p < 4) error("short unicode escape");
        unsigned v = 0;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else error("bad unicode escape");
        }
        return v;
    }
    static void utf8(std::string &out, unsigned cp) {
        if (cp < 0x80) out.push_back((char)cp);
        else if (cp < 0x800) {
            out.push_back((char)(0xc0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3f)));
        } else if (cp < 0x10000) {
            out.push_back((char)(0xe0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (cp & 0x3f)));
        } else {
            out.push_back((char)(0xf0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3f)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (cp & 0x3f)));
        }
    }
    std::string string() {
        p++;  // opening quote
        std::string out;
        for (;;) {
            const char *run = p;
            while (p < end && *p != '"' && *p != '\\' && (unsigned char)*p >= 0x20) p++;
            out.append(run, p - run);
            if (p >= end) error("unterminated string");
            if (*p == '"') {
                p++;
                return out;
            }
            if ((unsigned char)*p < 0x20) error("control character in string");
            p++;
            if (p >= end) error("unterminated escape");
            char c = *p++;
            switch (c) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                unsigned cp = hex4();
                if (cp >= 0xd800 && cp < 0xdc00 && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                    const char *save = p;
                    p += 2;
                    unsigned lo = hex4();
                    if (lo >= 0xdc00 && lo < 0xe000) cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                    else p = save;
                }
                utf8(out, cp);
                break;
            }
            default: error("bad escape");
            }
        }
    }
    Json number() {
        const char *start = p;
        bool integral = true;
        if (*p == '-') p++;
        if (p < end && *p == 'I' && literal("Infinity")) return Json(-INFINITY);
        while (p < end && *p >= '0' && *p <= '9') p++;
        if (p < end && *p == '.') {
            integral = false;
            p++;
            while (p < end && *p >= '0' && *p <= '9') p++;
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            integral = false;
            p++;
            if (p < end && (*p == '+' || *p == '-')) p++;
            while (p < end && *p >= '0' && *p <= '9') p++;
        }
        std::string text(start, p - start);
        if (text == "-" || text.empty()) error("bad number");
        if (integral) {
            errno = 0;
            long long v = std::strtoll(text.c_str(), nullptr, 10);
            if (!errno) return Json(v);
        }
        return Json(std::strtod(text.c_str(), nullptr));
    }
    Json value() {
        ws();
        if (p >= end) error("unexpected end");
        if (++depth > 512) error("nesting too deep");
        Json v;
        switch (*p) {
        case '{': {
            p++;
            Json::Members m;
            ws();
            if (p < end && *p == '}') {
                p++;
                v = Json(std::move(m));
                break;
            }
            for (;;) {
                ws();
                if (p >= end || *p != '"') error("expected key");
                std::string key = string();
                ws();
                if (p >= end || *p != ':') error("expected ':'");
                p++;
                Json item = value();
                // Python keeps the last duplicate.
                bool replaced = false;
                for (auto &e : m)
                    if (e.first == key) {
                        e.second = std::move(item);
                        replaced = true;
                        break;
                    }
                if (!replaced) m.emplace_back(std::move(key), std::move(item));
                ws();
                if (p < end && *p == ',') {
                    p++;
                    continue;
                }
                if (p < end && *p == '}') {
                    p++;
                    break;
                }
                error("expected ',' or '}'");
            }
            v = Json(std::move(m));
            break;
        }
        case '[': {
            p++;
            Json::Elements a;
            ws();
            if (p < end && *p == ']') {
                p++;
                v = Json(std::move(a));
                break;
            }
            for (;;) {
                a.push_back(value());
                ws();
                if (p < end && *p == ',') {
                    p++;
                    continue;
                }
                if (p < end && *p == ']') {
                    p++;
                    break;
                }
                error("expected ',' or ']'");
            }
            v = Json(std::move(a));
            break;
        }
        case '"': v = Json(string()); break;
        case 't':
            if (!literal("true")) error("bad literal");
            v = Json(true);
            break;
        case 'f':
            if (!literal("false")) error("bad literal");
            v = Json(false);
            break;
        case 'n':
            if (!literal("null")) error("bad literal");
            break;
        case 'N':
            if (!literal("NaN")) error("bad literal");
            v = Json(NAN);
            break;
        case 'I':
            if (!literal("Infinity")) error("bad literal");
            v = Json(INFINITY);
            break;
        default:
            if (*p == '-' || (*p >= '0' && *p <= '9')) v = number();
            else error("unexpected character");
        }
        depth--;
        return v;
    }
};
}  // namespace

Json Json::parse(std::string_view text) {
    Parser parser{text.data(), text.data() + text.size()};
    Json v = parser.value();
    parser.ws();
    if (parser.p != parser.end) throw JsonError("JSON parse error: trailing data");
    return v;
}

}  // namespace npps
