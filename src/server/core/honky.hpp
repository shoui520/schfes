#pragma once
// SIF game file encryption (HonokaMiku v3/v4, JP keys), after HonkyPy by
// Dark Energy Processor (MIT). Encryption and decryption are the same XOR.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace npps::honky {

constexpr size_t HEADER_SIZE = 16;

struct Context {
    uint32_t key, init_key, a, c, shift;
    // Applies the key stream in place, continuing from the previous call.
    void apply(void *data, size_t n);
};

// Sets up decryption of `basename` (path components are ignored) from the
// file's first 16 bytes. Returns false when the header does not match.
bool decrypt_setup(std::string_view filename, const unsigned char *header, Context &out);
// Version 3 encryption; writes the 16-byte header.
void encrypt_setup_v3(std::string_view filename, unsigned char header[HEADER_SIZE], Context &out);
bool is_sqlite(const unsigned char *header, size_t n);

}  // namespace npps::honky
