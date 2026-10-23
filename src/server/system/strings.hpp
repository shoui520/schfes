#pragma once
// npps4/strings/__init__.py
#include "app/context.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace npps::strings {

// tuple[str, str | None]: (jp, en)
using String = std::pair<std::string, std::optional<std::string>>;
// int | str format argument
using Arg = std::variant<int64_t, std::string>;

// Indexed by server string id; gaps are ("", None).
extern const std::vector<String> SERVER_STRINGS;
// game_mater.STRINGS: (string_key, string_value) -> (string_label, string_label_en), loaded on first use.
const std::map<std::pair<std::string, std::string>, String> &CLIENT_STRINGS();

// Throws std::out_of_range (KeyError) when missing.
String get(const std::string &string_key, int64_t string_value);
String get(const std::string &string_key, const std::string &string_value);
std::string get(const std::string &string_key, int64_t string_value, Language lang);
std::string get(const std::string &string_key, const std::string &string_value, Language lang);
std::string get(const std::string &string_key, int64_t string_value, const Context &context);
std::string get(const std::string &string_key, const std::string &string_value, const Context &context);

std::string select(const String &string, Language lang);
std::string select(const String &string, const Context &context);

// {{x}} placeholders.
std::string format_mapped(const std::string &string, const std::map<std::string, Arg> &kwargs);
String format_mapped(const String &string, const std::map<std::string, Arg> &kwargs);

// %1$s / %2$d / %3$x; throws std::invalid_argument (ValueError).
std::string format_positional(const std::string &string, const std::vector<Arg> &args);
String format_positional(const String &string, const std::vector<Arg> &args);

// string % args (printf-style %s and %d).
std::string format_simple(const std::string &string, const std::vector<Arg> &args);
String format_simple(const String &string, const std::vector<Arg> &args);

// Variadic conveniences: format_simple(s, month, day).
template <typename S, typename... A> auto format_positional(const S &string, const A &...args) {
    return format_positional(string, std::vector<Arg>{Arg(args)...});
}
template <typename S, typename... A> auto format_simple(const S &string, const A &...args) {
    return format_simple(string, std::vector<Arg>{Arg(args)...});
}

}  // namespace npps::strings
