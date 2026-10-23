// npps4/strings/__init__.py
#include "system/strings.hpp"

#include <stdexcept>

namespace npps::strings {

// When adding new strings, please put the index (0-based) and append them!
// FIXME: Proper EN translation.
const std::vector<String> SERVER_STRINGS = {
    {"Rankを%d にしよう!の報酬", "Let's make the Rank %d!"},
    {"ライプの報醂", "Live Show Reward"},
};

const std::map<std::pair<std::string, std::string>, String> &CLIENT_STRINGS() {
    static const std::map<std::pair<std::string, std::string>, String> strings = [] {
        std::map<std::pair<std::string, std::string>, String> out;
        auto rows = databases().game_mater.select<db::game_mater::Strings>("");
        for (const auto *r : rows) out[{r->string_key, r->string_value}] = {r->string_label, r->string_label_en};
        databases().game_mater.end_request();
        return out;
    }();
    return strings;
}

String get(const std::string &string_key, const std::string &string_value) {
    auto it = CLIENT_STRINGS().find({string_key, string_value});
    if (it == CLIENT_STRINGS().end()) throw std::out_of_range("string not found: " + string_key + ", " + string_value);
    return it->second;
}
String get(const std::string &string_key, int64_t string_value) { return get(string_key, std::to_string(string_value)); }

std::string get(const std::string &string_key, int64_t string_value, Language lang) {
    return select(get(string_key, string_value), lang);
}
std::string get(const std::string &string_key, const std::string &string_value, Language lang) {
    return select(get(string_key, string_value), lang);
}
std::string get(const std::string &string_key, int64_t string_value, const Context &context) {
    return select(get(string_key, string_value), context.lang);
}
std::string get(const std::string &string_key, const std::string &string_value, const Context &context) {
    return select(get(string_key, string_value), context.lang);
}

std::string select(const String &string, Language lang) {
    if (lang == Language::en && string.second && !string.second->empty()) return *string.second;
    return string.first;
}
std::string select(const String &string, const Context &context) { return select(string, context.lang); }

static std::string arg_str(const Arg &a) { return std::holds_alternative<int64_t>(a) ? std::to_string(std::get<int64_t>(a)) : std::get<std::string>(a); }

// re.sub(r"\{\{([a-zA-Z0-9_])\}\}", ...): the pattern matches a single character name.
std::string format_mapped(const std::string &string, const std::map<std::string, Arg> &kwargs) {
    std::string out;
    size_t i = 0;
    auto is_name = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; };
    while (i < string.size()) {
        if (i + 5 <= string.size() && string[i] == '{' && string[i + 1] == '{' && is_name(string[i + 2]) &&
            string[i + 3] == '}' && string[i + 4] == '}') {
            auto it = kwargs.find(std::string(1, string[i + 2]));
            if (it != kwargs.end()) out += arg_str(it->second);
            else out += "{{" + string.substr(i, 5) + "}}";
            i += 5;
        } else
            out += string[i++];
    }
    return out;
}
String format_mapped(const String &string, const std::map<std::string, Arg> &kwargs) {
    return {format_mapped(string.first, kwargs),
            string.second ? std::optional<std::string>(format_mapped(*string.second, kwargs)) : std::nullopt};
}

// re.sub(r"%\d+\$[a-z]", ...)
std::string format_positional(const std::string &string, const std::vector<Arg> &args) {
    std::string out;
    size_t i = 0;
    while (i < string.size()) {
        size_t j = i + 1;
        if (string[i] == '%' && j < string.size() && string[j] >= '0' && string[j] <= '9') {
            while (j < string.size() && string[j] >= '0' && string[j] <= '9') j++;
            if (j + 1 < string.size() && string[j] == '$' && string[j + 1] >= 'a' && string[j + 1] <= 'z') {
                int64_t pos = std::stoll(string.substr(i + 1, j - i - 1));
                char spec = string[j + 1];
                // args[pos - 1]; pos == 0 indexes from the end like Python
                int64_t idx = pos - 1;
                if (idx < 0) idx += (int64_t)args.size();
                if (idx < 0 || idx >= (int64_t)args.size()) throw std::out_of_range("tuple index out of range");
                const Arg &value = args[(size_t)idx];
                bool is_int = std::holds_alternative<int64_t>(value);
                if (spec == 's') {
                    if (is_int) throw std::invalid_argument("invalid format specifier index " + std::to_string(pos));
                    out += std::get<std::string>(value);
                } else if (spec == 'd' || spec == 'x') {
                    if (!is_int) throw std::invalid_argument("invalid format specifier index " + std::to_string(pos));
                    // "x" is str.format("%x", k) in NPPS4, which yields the literal "%x".
                    if (spec == 'd') out += std::to_string(std::get<int64_t>(value));
                    else out += "%x";
                } else
                    throw std::invalid_argument(std::string("unknown format specifier '") + spec + "'");
                i = j + 2;
                continue;
            }
        }
        out += string[i++];
    }
    return out;
}
String format_positional(const String &string, const std::vector<Arg> &args) {
    return {format_positional(string.first, args),
            string.second ? std::optional<std::string>(format_positional(*string.second, args)) : std::nullopt};
}

// Python printf-style `string % args` for int and str arguments.
std::string format_simple(const std::string &string, const std::vector<Arg> &args) {
    std::string out;
    size_t next = 0, i = 0;
    while (i < string.size()) {
        if (string[i] != '%') {
            out += string[i++];
            continue;
        }
        i++;
        std::string flags;
        while (i < string.size() && std::string("-+ 0#").find(string[i]) != std::string::npos) flags += string[i++];
        int width = -1, prec = -1;
        if (i < string.size() && string[i] == '*') {
            if (next >= args.size() || !std::holds_alternative<int64_t>(args[next]))
                throw std::invalid_argument("* wants int");
            width = (int)std::get<int64_t>(args[next++]);
            i++;
        } else {
            while (i < string.size() && string[i] >= '0' && string[i] <= '9') width = (width < 0 ? 0 : width) * 10 + (string[i++] - '0');
        }
        if (i < string.size() && string[i] == '.') {
            i++;
            prec = 0;
            while (i < string.size() && string[i] >= '0' && string[i] <= '9') prec = prec * 10 + (string[i++] - '0');
        }
        if (i >= string.size()) throw std::invalid_argument("incomplete format");
        char conv = string[i++];
        if (conv == '%') {
            out += '%';
            continue;
        }
        if (next >= args.size()) throw std::invalid_argument("not enough arguments for format string");
        const Arg &a = args[next++];
        std::string body;
        bool numeric = false;
        if (conv == 's' || conv == 'r' || conv == 'a') {
            body = arg_str(a);
            if (conv != 's' && !std::holds_alternative<int64_t>(a)) body = "'" + body + "'";
            if (prec >= 0 && (size_t)prec < body.size()) body.resize((size_t)prec);
        } else if (conv == 'd' || conv == 'i' || conv == 'u' || conv == 'x' || conv == 'X' || conv == 'o') {
            if (!std::holds_alternative<int64_t>(a))
                throw std::invalid_argument(std::string("%") + conv + " format: a real number is required");
            int64_t v = std::get<int64_t>(a);
            numeric = true;
            bool neg = v < 0;
            uint64_t mag = neg ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
            int base = (conv == 'x' || conv == 'X') ? 16 : conv == 'o' ? 8 : 10;
            const char *digits = conv == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            std::string d;
            do d.insert(d.begin(), digits[mag % base]); while ((mag /= base) != 0);
            if (prec >= 0 && (int)d.size() < prec) d.insert(0, (size_t)prec - d.size(), '0');
            std::string prefix;
            if (flags.find('#') != std::string::npos && base != 10) prefix = base == 8 ? "0o" : conv == 'X' ? "0X" : "0x";
            std::string sign = neg ? "-" : flags.find('+') != std::string::npos ? "+" : flags.find(' ') != std::string::npos ? " " : "";
            bool zero = flags.find('0') != std::string::npos && flags.find('-') == std::string::npos && prec < 0;
            if (zero && width > (int)(sign.size() + prefix.size() + d.size()))
                d.insert(0, (size_t)width - sign.size() - prefix.size() - d.size(), '0');
            body = sign + prefix + d;
        } else
            throw std::invalid_argument(std::string("unsupported format character '") + conv + "'");
        if (width > 0 && (int)body.size() < width) {
            (void)numeric;
            if (flags.find('-') != std::string::npos) body.append((size_t)width - body.size(), ' ');
            else body.insert(0, (size_t)width - body.size(), ' ');
        }
        out += body;
    }
    if (next < args.size()) throw std::invalid_argument("not all arguments converted during string formatting");
    return out;
}
String format_simple(const String &string, const std::vector<Arg> &args) {
    return {format_simple(string.first, args),
            string.second ? std::optional<std::string>(format_simple(*string.second, args)) : std::nullopt};
}

}  // namespace npps::strings
