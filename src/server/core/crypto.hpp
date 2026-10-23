#pragma once
#include "util.hpp"

namespace npps::crypto {

// Loads the PEM private key used for X-Message-Sign and authkey.
void load_server_key(std::string_view pem, std::string_view password = {});
// base64(PKCS#1 v1.5 RSA-SHA1(content [+ xmc hex]))
std::string sign_message(std::string_view content, const std::string *request_xmc_hex);
// PKCS#1 v1.5 RSA decryption; empty result on failure (PyCryptodome sentinel=None).
Bytes decrypt_rsa(std::string_view data);
// AES-CBC with the IV prefix; PKCS#7 padding stripped like util.decrypt_aes.
Bytes decrypt_aes(std::string_view key, std::string_view data);
Bytes encrypt_aes(std::string_view key, std::string_view plain);
Bytes hmac_sha1(std::string_view message, std::string_view key);
Bytes hmac_sha512(std::string_view message, std::string_view key);
Bytes sha1(std::string_view data);
Bytes sha256(std::string_view data);
Bytes sha512(std::string_view data);
Bytes md5(std::string_view data);
bool constant_time_equal(std::string_view a, std::string_view b);

}  // namespace npps::crypto
