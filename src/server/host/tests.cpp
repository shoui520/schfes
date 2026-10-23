// Unit checks for pieces that must match CPython exactly.
#include "core/json.hpp"
#include "core/util.hpp"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                 \
        }                                                               \
    } while (0)

int main() {
    using npps::util::PyRandom;
    PyRandom r;
    r.seed(12345);
    CHECK(r.getrandbits(32) == 1789368711u);
    CHECK(r.random() == 0.7326852754516092);
    CHECK(r.randint(1, 100) == 39);
    std::vector<int> pop{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<int> w{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CHECK((r.choices(pop, w, 5) == std::vector<int>{9, 9, 4, 6, 5}));
    CHECK(npps::util::hex(r.randbytes(7)) == "b1e3c9dfe9d46e");
    std::vector<char> letters{'a', 'b', 'c', 'd', 'e', 'f'};
    CHECK(r.choice(letters) == 'c');
    std::vector<int> l = pop;
    r.shuffle(l);
    CHECK((l == std::vector<int>{1, 4, 6, 3, 0, 5, 7, 9, 2, 8}));
    r.seed_bytes("hello");
    CHECK(r.getrandbits(32) == 1519453933u);
    printf("%s\n", failures ? "FAILED" : "ok");
    return failures != 0;
}
