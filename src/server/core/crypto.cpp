#include "crypto.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <memory>
#include <mutex>
#include <stdexcept>

namespace npps::crypto {

static RSA *server_key;
static std::mutex rsa_lock;

void load_server_key(std::string_view pem, std::string_view password) {
    BIO *bio = BIO_new_mem_buf(pem.data(), (int)pem.size());
    std::string pass(password);
    RSA *key = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, pass.empty() ? nullptr : (void *)pass.c_str());
    BIO_free(bio);
    if (!key) throw std::runtime_error("invalid server private key");
    std::lock_guard<std::mutex> guard(rsa_lock);
    if (server_key) RSA_free(server_key);
    server_key = key;
}

static RSA *key() {
    if (!server_key) throw std::runtime_error("server key not loaded");
    return server_key;
}

std::string sign_message(std::string_view content, const std::string *xmc) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, content.data(), content.size());
    if (xmc) SHA1_Update(&ctx, xmc->data(), xmc->size());
    SHA1_Final(digest, &ctx);
    std::lock_guard<std::mutex> guard(rsa_lock);
    std::string sig(RSA_size(key()), '\0');
    unsigned len = 0;
    if (!RSA_sign(NID_sha1, digest, sizeof(digest), (unsigned char *)&sig[0], &len, key()))
        throw std::runtime_error("RSA signing failed");
    sig.resize(len);
    return util::base64_encode(sig);
}

Bytes decrypt_rsa(std::string_view data) {
    std::lock_guard<std::mutex> guard(rsa_lock);
    Bytes out(RSA_size(key()), '\0');
    int n = RSA_private_decrypt((int)data.size(), (const unsigned char *)data.data(), (unsigned char *)&out[0], key(),
                                RSA_PKCS1_PADDING);
    if (n < 0) return {};
    out.resize(n);
    return out;
}

static const EVP_CIPHER *aes_cipher(size_t key_size) {
    switch (key_size) {
    case 16: return EVP_aes_128_cbc();
    case 24: return EVP_aes_192_cbc();
    case 32: return EVP_aes_256_cbc();
    default: throw std::invalid_argument("invalid AES key size");
    }
}

Bytes decrypt_aes(std::string_view key, std::string_view data) {
    if (data.size() < 32 || data.size() % 16) throw std::invalid_argument("invalid AES ciphertext");
    std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX *)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    Bytes out(data.size(), '\0');
    int n = 0, m = 0;
    if (!EVP_DecryptInit_ex(ctx.get(), aes_cipher(key.size()), nullptr, (const unsigned char *)key.data(),
                            (const unsigned char *)data.data()) ||
        !EVP_CIPHER_CTX_set_padding(ctx.get(), 0) ||
        !EVP_DecryptUpdate(ctx.get(), (unsigned char *)&out[0], &n, (const unsigned char *)data.data() + 16,
                           (int)data.size() - 16) ||
        !EVP_DecryptFinal_ex(ctx.get(), (unsigned char *)&out[n], &m))
        throw std::invalid_argument("AES decryption failed");
    out.resize(n + m);
    // data[:-data[-1]], without validating the padding bytes.
    size_t pad = out.empty() ? 0 : (unsigned char)out.back();
    out.resize(pad > out.size() ? 0 : out.size() - pad);
    return out;
}

Bytes encrypt_aes(std::string_view key, std::string_view plain) {
    Bytes iv = util::randbytes(16);
    std::unique_ptr<EVP_CIPHER_CTX, void (*)(EVP_CIPHER_CTX *)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    Bytes out(plain.size() + 32, '\0');
    int n = 0, m = 0;
    if (!EVP_EncryptInit_ex(ctx.get(), aes_cipher(key.size()), nullptr, (const unsigned char *)key.data(),
                            (const unsigned char *)iv.data()) ||
        !EVP_EncryptUpdate(ctx.get(), (unsigned char *)&out[0], &n, (const unsigned char *)plain.data(), (int)plain.size()) ||
        !EVP_EncryptFinal_ex(ctx.get(), (unsigned char *)&out[n], &m))
        throw std::runtime_error("AES encryption failed");
    out.resize(n + m);
    return iv + out;
}

static Bytes hmac(const EVP_MD *md, std::string_view message, std::string_view key) {
    unsigned char out[EVP_MAX_MD_SIZE];
    unsigned len = 0;
    if (!HMAC(md, key.data(), (int)key.size(), (const unsigned char *)message.data(), message.size(), out, &len))
        throw std::runtime_error("HMAC failed");
    return Bytes((const char *)out, len);
}

Bytes hmac_sha1(std::string_view message, std::string_view key) { return hmac(EVP_sha1(), message, key); }
Bytes hmac_sha512(std::string_view message, std::string_view key) { return hmac(EVP_sha512(), message, key); }

Bytes sha1(std::string_view data) {
    unsigned char out[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *)data.data(), data.size(), out);
    return Bytes((const char *)out, sizeof(out));
}

Bytes sha512(std::string_view data) {
    unsigned char out[SHA512_DIGEST_LENGTH];
    SHA512((const unsigned char *)data.data(), data.size(), out);
    return Bytes((const char *)out, sizeof(out));
}
static const bool sha512_hook_installed = (util::crypto_sha512_hook = sha512, true);

Bytes sha256(std::string_view data) {
    unsigned char out[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)data.data(), data.size(), out);
    return Bytes((const char *)out, sizeof(out));
}

Bytes md5(std::string_view data) {
    unsigned char out[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *)data.data(), data.size(), out);
    return Bytes((const char *)out, sizeof(out));
}

bool constant_time_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    unsigned char d = 0;
    for (size_t i = 0; i < a.size(); i++) d |= a[i] ^ b[i];
    return d == 0;
}

}  // namespace npps::crypto
