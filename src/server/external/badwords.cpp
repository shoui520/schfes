// external/badwords.py
#include "external/badwords.hpp"

#include "data/data.hpp"

namespace npps::external::badwords {

// Decodes one code point; invalid bytes are passed through as themselves.
static uint32_t next_code_point(const std::string &s, size_t &i) {
    unsigned char c = (unsigned char)s[i];
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (i + extra >= s.size()) extra = 0;
    uint32_t cp = extra == 0 ? c : (c & (0x3F >> extra));
    for (int k = 1; k <= extra; k++) cp = cp << 6 | ((unsigned char)s[i + k] & 0x3F);
    i += extra + 1;
    return cp;
}

// re.compile(r"\s+") for str patterns (Unicode whitespace).
static bool is_space(uint32_t c) {
    return (c >= 9 && c <= 13) || (c >= 0x1c && c <= 0x20) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}

// str.lower() + removal of whitespace. Only ASCII is case-folded.
static std::string normalize(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        size_t start = i;
        uint32_t cp = next_code_point(text, i);
        if (is_space(cp)) continue;
        if (cp >= 'A' && cp <= 'Z') out += (char)(cp + 32);
        else out.append(text, start, i - start);
    }
    return out;
}

bool has_badwords(Context &, const std::string &text) {
    std::string new_text = normalize(text);
    const data::ServerData &server_data = data::get();
    for (const std::string &badword : server_data.badwords)
        if (new_text.find(badword) != std::string::npos) return true;
    return false;
}

}  // namespace npps::external::badwords
