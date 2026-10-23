#include "util.hpp"

#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <ctime>
#include <mutex>
#include <stdexcept>

#ifdef __vita__
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#else
#include <chrono>
#include <fstream>
#endif

namespace npps::util {

static int64_t frozen_time = 0;
int64_t time() { return frozen_time ? frozen_time : (int64_t)::time(nullptr); }
void set_time_for_testing(int64_t t) { frozen_time = t; }

uint64_t monotonic_us() {
#ifdef __vita__
    return sceKernelGetProcessTimeWide();
#else
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

static Bytes system_randbytes(size_t n) {
    Bytes out(n, '\0');
#ifdef __vita__
    // The kernel RNG returns at most 64 bytes per call.
    for (size_t done = 0; done < n;) {
        size_t chunk = n - done < 64 ? n - done : 64;
        if (sceKernelGetRandomNumber(&out[done], chunk) < 0) throw std::runtime_error("kernel RNG failed");
        done += chunk;
    }
#else
    static std::ifstream urandom("/dev/urandom", std::ios::binary);
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    if (!urandom.read(&out[0], n)) throw std::runtime_error("urandom failed");
#endif
    return out;
}

// MT19937 with CPython's random.Random algorithms on top, so a seeded
// test run draws exactly what NPPS4 draws.
uint32_t PyRandom::next32() {
    if (index_ >= 624) {
        static const uint32_t mag01[2] = {0u, 0x9908b0dfu};
        int k;
        for (k = 0; k < 624 - 397; k++) {
            uint32_t y = (mt_[k] & 0x80000000u) | (mt_[k + 1] & 0x7fffffffu);
            mt_[k] = mt_[k + 397] ^ (y >> 1) ^ mag01[y & 1];
        }
        for (; k < 623; k++) {
            uint32_t y = (mt_[k] & 0x80000000u) | (mt_[k + 1] & 0x7fffffffu);
            mt_[k] = mt_[k + (397 - 624)] ^ (y >> 1) ^ mag01[y & 1];
        }
        uint32_t y = (mt_[623] & 0x80000000u) | (mt_[0] & 0x7fffffffu);
        mt_[623] = mt_[396] ^ (y >> 1) ^ mag01[y & 1];
        index_ = 0;
    }
    uint32_t y = mt_[index_++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y;
}

void PyRandom::seed_words(const std::vector<uint32_t> &key) {
    mt_[0] = 19650218u;
    for (int i = 1; i < 624; i++) mt_[i] = 1812433253u * (mt_[i - 1] ^ (mt_[i - 1] >> 30)) + i;
    size_t i = 1, j = 0, n = key.empty() ? 1 : key.size();
    for (size_t k = 624 > n ? 624 : n; k; k--) {
        mt_[i] = (mt_[i] ^ ((mt_[i - 1] ^ (mt_[i - 1] >> 30)) * 1664525u)) + (key.empty() ? 0 : key[j]) + (uint32_t)j;
        i++;
        j++;
        if (i >= 624) {
            mt_[0] = mt_[623];
            i = 1;
        }
        if (j >= n) j = 0;
    }
    for (size_t k = 623; k; k--) {
        mt_[i] = (mt_[i] ^ ((mt_[i - 1] ^ (mt_[i - 1] >> 30)) * 1566083941u)) - (uint32_t)i;
        i++;
        if (i >= 624) {
            mt_[0] = mt_[623];
            i = 1;
        }
    }
    mt_[0] = 0x80000000u;
    index_ = 624;
}

void PyRandom::seed(uint64_t value) {
    std::vector<uint32_t> key;
    while (value) {
        key.push_back((uint32_t)value);
        value >>= 32;
    }
    seed_words(key);
}

void PyRandom::seed_bytes(std::string_view data) {
    // random.seed(bytes): int.from_bytes(data + sha512(data), "big").
    Bytes all(data);
    all += crypto_sha512_hook ? crypto_sha512_hook(data) : Bytes();
    std::vector<uint32_t> key;
    for (size_t end = all.size(); end > 0;) {
        size_t begin = end >= 4 ? end - 4 : 0;
        uint32_t w = 0;
        for (size_t i = begin; i < end; i++) w = (w << 8) | (unsigned char)all[i];
        key.push_back(w);
        end = begin;
    }
    while (key.size() > 1 && key.back() == 0) key.pop_back();
    seed_words(key);
}

uint64_t PyRandom::getrandbits(int k) {
    if (k <= 0) return 0;
    if (k <= 32) return next32() >> (32 - k);
    uint64_t lo = next32();
    uint64_t hi = next32() >> (64 - k);
    return lo | (hi << 32);
}

uint64_t PyRandom::randbelow(uint64_t n) {
    if (n == 0) return 0;
    int k = 64 - __builtin_clzll(n);
    uint64_t r = getrandbits(k);
    while (r >= n) r = getrandbits(k);
    return r;
}

double PyRandom::random() {
    uint32_t a = next32() >> 5, b = next32() >> 6;
    return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
}

int64_t PyRandom::randint(int64_t lo, int64_t hi) {
    if (hi < lo) throw std::invalid_argument("empty range for randrange()");
    return lo + (int64_t)randbelow((uint64_t)(hi - lo) + 1);
}

Bytes PyRandom::randbytes(size_t n) {
    // getrandbits(n * 8).to_bytes(n, "little")
    Bytes out(n, '\0');
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        uint32_t w = next32();
        memcpy(&out[i], &w, 4);
    }
    if (i < n) {
        uint32_t w = next32() >> (32 - 8 * (n - i));
        for (size_t j = 0; i < n; i++, j++) out[i] = (char)(w >> (8 * j));
    }
    return out;
}

Bytes (*crypto_sha512_hook)(std::string_view) = nullptr;

static PyRandom &global_rng() {
    static PyRandom rng = [] {
        PyRandom r;
        Bytes seed = system_randbytes(32);
        std::vector<uint32_t> key(8);
        memcpy(key.data(), seed.data(), 32);
        r.seed_words(key);
        return r;
    }();
    return rng;
}

static bool deterministic = false;

PyRandom &rng() { return global_rng(); }

void seed_for_testing(uint64_t seed) {
    deterministic = true;
    global_rng().seed(seed);
}

Bytes randbytes(size_t n) { return deterministic ? global_rng().randbytes(n) : system_randbytes(n); }
int64_t randint(int64_t lo, int64_t hi) { return global_rng().randint(lo, hi); }
double random() { return global_rng().random(); }

static void default_sink(const char *line) { fprintf(stderr, "%s\n", line); }
static void (*log_sink)(const char *) = default_sink;
void set_log_sink(void (*sink)(const char *line)) { log_sink = sink ? sink : default_sink; }

void vlog(const char *fmt, va_list args) {
    char buf[2048];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    if (n < 0) return;
    log_sink(buf);
}

void log(const char *fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vlog(fmt, a);
    va_end(a);
}

std::string format(const char *fmt, ...) {
    va_list a, b;
    va_start(a, fmt);
    va_copy(b, a);
    int n = vsnprintf(nullptr, 0, fmt, a);
    va_end(a);
    std::string out(n > 0 ? n : 0, '\0');
    if (n > 0) vsnprintf(&out[0], n + 1, fmt, b);
    va_end(b);
    return out;
}

static std::string b64(std::string_view data, const char *alphabet) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    const unsigned char *p = (const unsigned char *)data.data();
    size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        uint32_t v = p[i] << 16 | p[i + 1] << 8 | p[i + 2];
        out += alphabet[v >> 18];
        out += alphabet[(v >> 12) & 63];
        out += alphabet[(v >> 6) & 63];
        out += alphabet[v & 63];
    }
    if (i < data.size()) {
        uint32_t v = p[i] << 16 | (i + 1 < data.size() ? p[i + 1] << 8 : 0);
        out += alphabet[v >> 18];
        out += alphabet[(v >> 12) & 63];
        out += i + 1 < data.size() ? alphabet[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

std::string base64_encode(std::string_view data) {
    return b64(data, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
}
std::string base64_urlsafe_encode(std::string_view data) {
    return b64(data, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
}

Bytes base64_decode(std::string_view text) {
    Bytes out;
    out.reserve(text.size() / 4 * 3);
    uint32_t acc = 0;
    int bits = 0, pad = 0;
    for (char ch : text) {
        int v;
        if (ch >= 'A' && ch <= 'Z') v = ch - 'A';
        else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
        else if (ch >= '0' && ch <= '9') v = ch - '0' + 52;
        else if (ch == '+' || ch == '-') v = 62;
        else if (ch == '/' || ch == '_') v = 63;
        else if (ch == '=') {
            pad++;
            continue;
        } else if (ch == '\n' || ch == '\r') continue;
        else throw std::invalid_argument("invalid base64 character");
        if (pad) throw std::invalid_argument("invalid base64 padding");
        acc = acc << 6 | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += (char)(acc >> bits);
        }
    }
    if (bits >= 6) throw std::invalid_argument("truncated base64");
    return out;
}

std::string hex(std::string_view data, bool upper) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (unsigned char c : data) {
        out += digits[c >> 4];
        out += digits[c & 15];
    }
    return out;
}

Bytes xorbytes(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) throw std::invalid_argument("size does not match");
    Bytes out(a.size(), '\0');
    for (size_t i = 0; i < a.size(); i++) out[i] = a[i] ^ b[i];
    return out;
}

std::pair<int, int> parse_sif_version(std::string_view version) {
    size_t dot = version.find('.');
    if (dot == std::string_view::npos) throw std::invalid_argument("invalid version");
    return {std::stoi(std::string(version.substr(0, dot))), std::stoi(std::string(version.substr(dot + 1)))};
}

std::string sif_version_string(std::pair<int, int> v) { return format("%d.%d", v.first, v.second); }

// Civil calendar from days since 1970-01-01 (Howard Hinnant's algorithm).
static void civil(int64_t days, int &y, int &m, int &d) {
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned)(days - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y = (int)(yoe + era * 400) + (m <= 2);
}

static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

DateTime datetime(int64_t ts) {
    int64_t local = ts + OFFSET_JST;
    int64_t days = floordiv(local, 86400), secs = local - days * 86400;
    DateTime dt;
    civil(days, dt.year, dt.month, dt.day);
    dt.hour = (int)(secs / 3600);
    dt.minute = (int)(secs / 60 % 60);
    dt.second = (int)(secs % 60);
    dt.weekday = (int)pymod(days + 3, 7);  // 1970-01-01 was a Thursday
    return dt;
}

int64_t datetime_timestamp(int year, int month, int day, int hour, int minute, int second) {
    return days_from_civil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second - OFFSET_JST;
}

std::string timestamp_to_datetime(int64_t ts) {
    DateTime d = datetime(ts);
    return format("%04d-%02d-%02d %02d:%02d:%02d", d.year, d.month, d.day, d.hour, d.minute, d.second);
}

int64_t datetime_to_timestamp(std::string_view text) {
    std::string s(text);
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) throw std::invalid_argument("empty datetime");
    s = s.substr(a, b - a + 1);
    int y, mo, d, h, mi, se;
    char tail;
    if (sscanf(s.c_str(), "%d-%d-%d %d:%d:%d%c", &y, &mo, &d, &h, &mi, &se, &tail) != 6 &&
        sscanf(s.c_str(), "%d/%d/%d %d:%d:%d%c", &y, &mo, &d, &h, &mi, &se, &tail) != 6)
        throw std::invalid_argument("invalid datetime '" + s + "'");
    return datetime_timestamp(y, mo, d, h, mi, se);
}

int64_t get_days_since_unix(int64_t ts, int64_t offset) { return floordiv(ts + offset, 86400); }

int64_t get_next_day_timestamp(int64_t ts, int ndays, int64_t offset) {
    return (get_days_since_unix(ts, offset) + ndays) * 86400 - offset;
}

int64_t get_weeks_since_unix(int64_t ts, int64_t offset, int dow) {
    int64_t w = floordiv(get_days_since_unix(ts, offset) + 4 - dow, 7);
    return w < 0 ? 0 : w;
}

int get_months_since_unix(int64_t ts) { return datetime(ts).month; }

uint32_t java_hash_code(std::string_view text, uint32_t mask) {
    // Python iterates code points; decode UTF-8.
    uint32_t h = 0;
    const unsigned char *p = (const unsigned char *)text.data(), *end = p + text.size();
    while (p < end) {
        uint32_t cp = *p++;
        int extra = cp >= 0xf0 ? 3 : cp >= 0xe0 ? 2 : cp >= 0xc0 ? 1 : 0;
        if (extra) cp &= 0x3f >> extra;
        while (extra-- && p < end) cp = cp << 6 | (*p++ & 0x3f);
        h = (h * 31 + cp) & mask;
    }
    return h;
}

int64_t pyround(double value) { return (int64_t)std::nearbyint(value); }

double pyround(double value, int ndigits) {
    // CPython rounds the correctly rounded decimal string; this matches it
    // for the magnitudes the game uses.
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", ndigits, value);
    return strtod(buf, nullptr);
}

int64_t ceil_mul(int64_t value, double multiplier) {
    double v = (double)value * multiplier;
    int64_t i = (int64_t)v;
    return (double)i < v ? i + 1 : i;
}

std::vector<std::string> split(std::string_view text, char separator) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t at = text.find(separator, start);
        out.emplace_back(text.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start));
        if (at == std::string_view::npos) return out;
        start = at + 1;
    }
}

bool starts_with(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }

std::string url_decode(std::string_view text, bool plus_is_space) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '+' && plus_is_space) out += ' ';
        else if (c == '%' && i + 2 < text.size() && isxdigit((unsigned char)text[i + 1]) && isxdigit((unsigned char)text[i + 2])) {
            out += (char)std::stoi(std::string(text.substr(i + 1, 2)), nullptr, 16);
            i += 2;
        } else out += c;
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> parse_qsl(std::string_view query) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const std::string &part : split(query, '&')) {
        if (part.empty()) continue;
        size_t eq = part.find('=');
        // keep_blank_values=False drops "k=" and bare "k".
        if (eq == std::string::npos || eq + 1 == part.size()) continue;
        out.emplace_back(url_decode(part.substr(0, eq)), url_decode(part.substr(eq + 1)));
    }
    return out;
}

}  // namespace npps::util
