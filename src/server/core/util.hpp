#pragma once
// Counterparts of npps4/util.py plus platform services.
#include <cstdarg>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace npps {

using Bytes = std::string;  // binary data

namespace util {

constexpr int64_t OFFSET_JST = 32400;

int64_t time();  // Unix seconds
void set_time_for_testing(int64_t t);  // 0 restores the clock
uint64_t monotonic_us();
// CPython random.Random (MT19937): same draws as Python for the same seed.
class PyRandom {
public:
    void seed(uint64_t value);          // random.seed(int)
    void seed_bytes(std::string_view);  // random.seed(bytes), version 2
    void seed_words(const std::vector<uint32_t> &key);
    uint32_t next32();
    uint64_t getrandbits(int k);  // k <= 64
    uint64_t randbelow(uint64_t n);
    double random();
    int64_t randint(int64_t lo, int64_t hi);
    Bytes randbytes(size_t n);
    template <typename T> const T &choice(const std::vector<T> &items) {
        if (items.empty()) throw std::out_of_range("Cannot choose from an empty sequence");
        return items[randbelow(items.size())];
    }
    // random.choices(population, k=k)
    template <typename T> std::vector<T> choices(const std::vector<T> &population, size_t k) {
        std::vector<T> out;
        double n = (double)population.size();
        for (size_t i = 0; i < k; i++) out.push_back(population[(size_t)(random() * n)]);
        return out;
    }
    // random.choices(population, weights, k=k)
    template <typename T, typename W>
    std::vector<T> choices(const std::vector<T> &population, const std::vector<W> &weights, size_t k) {
        std::vector<double> cum;
        double total = 0;
        for (const W &w : weights) cum.push_back(total += (double)w);
        if (cum.size() != population.size()) throw std::invalid_argument("The number of weights does not match the population");
        std::vector<T> out;
        size_t hi = population.size() - 1;
        for (size_t i = 0; i < k; i++) {
            double x = random() * total;
            size_t lo = 0, h = hi;  // bisect_right(cum, x, 0, hi)
            while (lo < h) {
                size_t mid = (lo + h) / 2;
                if (x < cum[mid]) h = mid;
                else lo = mid + 1;
            }
            out.push_back(population[lo]);
        }
        return out;
    }
    template <typename T> void shuffle(std::vector<T> &x) {
        for (size_t i = x.size(); i-- > 1;) std::swap(x[i], x[randbelow(i + 1)]);
    }

private:
    uint32_t mt_[624];
    int index_ = 625;
};
// Set by core/crypto.cpp; used by PyRandom::seed_bytes.
extern Bytes (*crypto_sha512_hook)(std::string_view);

// The server-wide generator (util.SYSRAND). Seeded from system entropy
// unless seed_for_testing() was called.
PyRandom &rng();
void seed_for_testing(uint64_t seed);
// util.randbytes: system entropy, or the seeded generator in test mode.
Bytes randbytes(size_t n);
int64_t randint(int64_t lo, int64_t hi);  // inclusive, random.randint
double random();                          // [0, 1)
template <typename T> const T &choice(const std::vector<T> &items) { return rng().choice(items); }

void log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vlog(const char *fmt, va_list args);
// Log sink; defaults to stderr. The Vita build routes it to loader.log.
void set_log_sink(void (*sink)(const char *line));

std::string format(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

std::string base64_encode(std::string_view data);
std::string base64_urlsafe_encode(std::string_view data);
// Accepts both alphabets; throws std::invalid_argument on malformed input.
Bytes base64_decode(std::string_view text);
std::string hex(std::string_view data, bool upper = false);
Bytes xorbytes(std::string_view a, std::string_view b);

std::pair<int, int> parse_sif_version(std::string_view version);
std::string sif_version_string(std::pair<int, int> version);

// JST calendar helpers (util.datetime and friends).
struct DateTime { int year, month, day, hour, minute, second, weekday; /* Monday = 0 */ };
DateTime datetime(int64_t ts);
int64_t datetime_timestamp(int year, int month, int day, int hour = 0, int minute = 0, int second = 0);
std::string timestamp_to_datetime(int64_t ts);  // "%Y-%m-%d %H:%M:%S" in JST
// Parses "%Y-%m-%d %H:%M:%S" or "%Y/%m/%d %H:%M:%S" as JST; throws on error.
int64_t datetime_to_timestamp(std::string_view text);
int64_t get_next_day_timestamp(int64_t ts, int ndays = 1, int64_t offset = OFFSET_JST);
int64_t get_days_since_unix(int64_t ts, int64_t offset = OFFSET_JST);
int64_t get_weeks_since_unix(int64_t ts, int64_t offset = OFFSET_JST, int dow = 1);
int get_months_since_unix(int64_t ts);

uint32_t java_hash_code(std::string_view text, uint32_t mask = 0xffffffffu);
// Python floor division and modulo.
inline int64_t floordiv(int64_t a, int64_t b) { int64_t q = a / b; return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q; }
inline int64_t pymod(int64_t a, int64_t b) { int64_t r = a % b; return (r != 0 && ((r < 0) != (b < 0))) ? r + b : r; }
// Python round(): half to even.
int64_t pyround(double value);
double pyround(double value, int ndigits);
// math.ceil(a * multiplier) as used by the gameplay multipliers.
int64_t ceil_mul(int64_t value, double multiplier);
template <typename T> T clamp(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }

std::vector<std::string> split(std::string_view text, char separator);
bool starts_with(std::string_view text, std::string_view prefix);
std::string url_decode(std::string_view text, bool plus_is_space = true);
// Python urllib.parse.parse_qsl
std::vector<std::pair<std::string, std::string>> parse_qsl(std::string_view query);

}  // namespace util
}  // namespace npps
