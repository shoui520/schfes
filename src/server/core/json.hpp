#pragma once
// Ordered JSON value. Objects keep insertion order, matching pydantic's
// model_dump, and dump() reproduces Python json.dumps byte for byte.
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace npps {

class JsonError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Json {
public:
    enum Type : uint8_t { Null, Bool, Int, Double, String, Array, Object };
    using Member = std::pair<std::string, Json>;
    using Members = std::vector<Member>;
    using Elements = std::vector<Json>;

    Json() noexcept {}
    Json(std::nullptr_t) noexcept {}
    Json(bool v) noexcept : type_(Bool) { u_.b = v; }
    Json(int v) noexcept : type_(Int) { u_.i = v; }
    Json(unsigned v) noexcept : type_(Int) { u_.i = v; }
    Json(long v) noexcept : type_(Int) { u_.i = v; }
    Json(unsigned long v) noexcept : type_(Int) { u_.i = (int64_t)v; }
    Json(long long v) noexcept : type_(Int) { u_.i = v; }
    Json(unsigned long long v) noexcept : type_(Int) { u_.i = (int64_t)v; }
    Json(double v) noexcept : type_(Double) { u_.d = v; }
    Json(float v) noexcept : type_(Double) { u_.d = v; }
    Json(const char *v) : type_(String) { u_.s = new std::string(v); }
    Json(std::string_view v) : type_(String) { u_.s = new std::string(v); }
    Json(const std::string &v) : type_(String) { u_.s = new std::string(v); }
    Json(std::string &&v) : type_(String) { u_.s = new std::string(std::move(v)); }
    Json(Elements &&v) : type_(Array) { u_.a = new Elements(std::move(v)); }
    Json(Members &&v) : type_(Object) { u_.o = new Members(std::move(v)); }
    template <typename T>
    Json(const std::vector<T> &v) : type_(Array) {
        u_.a = new Elements();
        u_.a->reserve(v.size());
        for (const auto &e : v) u_.a->emplace_back(e);
    }
    Json(const Json &other);
    Json(Json &&other) noexcept : type_(other.type_), u_(other.u_) { other.type_ = Null; }
    Json &operator=(const Json &other);
    Json &operator=(Json &&other) noexcept;
    ~Json() { release(); }

    static Json array() { return Json(Elements()); }
    static Json object() { return Json(Members()); }
    // Json::obj({{"a", 1}, {"b", "x"}})
    static Json obj(std::initializer_list<std::pair<const char *, Json>> members);
    static Json arr(std::initializer_list<Json> elements);
    static Json parse(std::string_view text);

    Type type() const noexcept { return type_; }
    bool is_null() const noexcept { return type_ == Null; }
    bool is_bool() const noexcept { return type_ == Bool; }
    bool is_int() const noexcept { return type_ == Int; }
    bool is_double() const noexcept { return type_ == Double; }
    bool is_number() const noexcept { return type_ == Int || type_ == Double; }
    bool is_string() const noexcept { return type_ == String; }
    bool is_array() const noexcept { return type_ == Array; }
    bool is_object() const noexcept { return type_ == Object; }

    // Strict accessors: throw JsonError on a type mismatch. as_int accepts
    // bools and integral doubles; as_double accepts ints.
    bool as_bool() const;
    int64_t as_int() const;
    double as_double() const;
    const std::string &as_string() const;
    Elements &elements();
    const Elements &elements() const;
    Members &members();
    const Members &members() const;

    // Pydantic lax-mode coercions for request fields ("12" -> 12, 1 -> true).
    int64_t to_int() const;
    double to_double() const;
    bool to_bool() const;
    std::string to_string() const;

    size_t size() const noexcept;
    bool empty() const noexcept { return size() == 0; }

    // Objects. operator[] inserts null when missing and turns null into an
    // object; at() throws when missing.
    Json &operator[](std::string_view key);
    Json &operator[](const char *key) { return (*this)[std::string_view(key)]; }
    const Json &at(std::string_view key) const;
    Json *find(std::string_view key) noexcept;
    const Json *find(std::string_view key) const noexcept;
    bool contains(std::string_view key) const noexcept { return find(key) != nullptr; }
    bool erase(std::string_view key);
    // Value of an optional key, or `fallback` when missing or null.
    int64_t get_int(std::string_view key, int64_t fallback) const;
    std::string get_string(std::string_view key, std::string_view fallback) const;
    bool get_bool(std::string_view key, bool fallback) const;

    // Arrays. push_back turns null into an array.
    Json &operator[](size_t index);
    const Json &operator[](size_t index) const;
    Json &operator[](int index) { return (*this)[(size_t)index]; }
    const Json &operator[](int index) const { return (*this)[(size_t)index]; }
    Json &push_back(Json value);
    void reserve(size_t n);

    bool operator==(const Json &other) const;
    bool operator!=(const Json &other) const { return !(*this == other); }

    // Python json.dumps(obj) formatting: ", " and ": " separators,
    // ensure_ascii escapes and repr() floats. With exclude_none, null object
    // members are dropped recursively (pydantic exclude_none).
    std::string dump(bool exclude_none = false) const;
    void dump_to(std::string &out, bool exclude_none = false) const;

private:
    void release() noexcept;
    Type type_ = Null;
    union U {
        bool b;
        int64_t i;
        double d;
        std::string *s;
        Elements *a;
        Members *o;
    } u_{};
};

const char *json_type_name(Json::Type type);
void json_escape_to(std::string &out, std::string_view text);
void json_double_to(std::string &out, double value);

}  // namespace npps
