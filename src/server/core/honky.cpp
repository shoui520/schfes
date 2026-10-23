#include "honky.hpp"

#include <openssl/md5.h>

#include <cstring>

namespace npps::honky {

static const char PREFIX[] = "Hello";
static const uint32_t KEY_TABLES_JP[64] = {
    1210253353, 1736710334, 1030507233, 1924017366, 1603299666, 1844516425, 1102797553, 32188137,
    782633907,  356258523,  957120135,  10030910,   811467044,  1226589197, 1303858438, 1423840583,
    756169139,  1304954701, 1723556931, 648430219,  1560506399, 1987934810, 305677577,  505363237,
    450129501,  1811702731, 2146795414, 842747461,  638394899,  51014537,   198914076,  120739502,
    1973027104, 586031952,  1484278592, 1560111926, 441007634,  1006001970, 2038250142, 232546121,
    827280557,  1307729428, 775964996,  483398502,  1724135019, 2125939248, 742088754,  1411519905,
    136462070,  1084053905, 2039157473, 1943671327, 650795184,  151139993,  1467120569, 1883837341,
    1249929516, 382015614,  1020618905, 1082135529, 870997426,  1221338057, 1623152467, 1020681319,
};
struct Lcg { uint32_t a, c, shift; };
static const Lcg LCG[4] = {{1103515245, 12345, 15}, {22695477, 1, 23}, {214013, 2531011, 24}, {65793, 4282663, 8}};

static std::string_view basename(std::string_view name) {
    size_t slash = name.find_last_of('/');
    return slash == std::string_view::npos ? name : name.substr(slash + 1);
}

static void digest(std::string_view base, unsigned char md5[16]) {
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, PREFIX, 5);
    MD5_Update(&ctx, base.data(), base.size());
    MD5_Final(md5, &ctx);
}

static uint32_t name_sum(std::string_view base) {
    uint32_t sum = 0;
    for (char c : std::string_view(PREFIX)) sum += (unsigned char)c;
    for (char c : base) sum += (unsigned char)c;
    return sum;
}

void Context::apply(void *data, size_t n) {
    unsigned char *p = (unsigned char *)data;
    uint32_t k = key;
    for (size_t i = 0; i < n; i++) {
        p[i] ^= (unsigned char)(k >> (shift & 31));
        k = k * a + c;
    }
    key = k;
}

static void init(Context &out, uint32_t key, const Lcg &lcg) {
    out.key = out.init_key = key;
    out.a = lcg.a;
    out.c = lcg.c;
    out.shift = lcg.shift;
}

bool decrypt_setup(std::string_view filename, const unsigned char *h, Context &out) {
    std::string_view base = basename(filename);
    unsigned char md5[16];
    digest(base, md5);
    if (h[0] != (unsigned char)~md5[4] || h[1] != (unsigned char)~md5[5] || h[2] != (unsigned char)~md5[6]) return false;
    if (h[7] < 2) {
        uint32_t key = KEY_TABLES_JP[name_sum(base) & 63];
        init(out, h[7] == 1 ? ~key : key, LCG[2]);
        return true;
    }
    if (h[7] == 2 && h[6] < 4) {
        init(out, (uint32_t)md5[8] << 24 | md5[9] << 16 | md5[10] << 8 | md5[11], LCG[h[6]]);
        return true;
    }
    return false;
}

void encrypt_setup_v3(std::string_view filename, unsigned char h[HEADER_SIZE], Context &out) {
    std::string_view base = basename(filename);
    unsigned char md5[16];
    digest(base, md5);
    uint32_t sum = name_sum(base);
    memset(h, 0, HEADER_SIZE);
    h[0] = ~md5[4];
    h[1] = ~md5[5];
    h[2] = ~md5[6];
    h[3] = 12;
    h[8] = sum >> 24;
    h[9] = sum >> 16;
    h[10] = sum >> 8;
    h[11] = sum;
    init(out, KEY_TABLES_JP[sum & 63], LCG[2]);
}

bool is_sqlite(const unsigned char *header, size_t n) { return n >= 16 && !memcmp(header, "SQLite format 3", 16); }

}  // namespace npps::honky
