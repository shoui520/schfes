// npps4/data/schema.py
#include "data/schema.hpp"

#include "core/crypto.hpp"

#include <openssl/evp.h>

#include <memory>
#include <stdexcept>

namespace npps::data::schema {

int64_t _internal_id(const HasIDString &item) { return (int64_t)util::java_hash_code(item.id_string); }
int64_t _internal_id(const StickerShop &item) { return (int64_t)util::java_hash_code(item.id_string); }

SerialCodeAction parse_serial_code_action(const Json &value) {
    // Discriminated by "type" (Literal["item"] / Literal["run"]); a missing type falls back to the first
    // member that validates, like a smart-mode union.
    const Json *type = value.is_object() ? value.find("type") : nullptr;
    if (type && type->is_string()) {
        if (type->as_string() == "item") return model::parse<SerialCodeGiveItem>(value);
        if (type->as_string() == "run") return model::parse<SerialCodeRunFunction>(value);
        throw error::ValidationError("Input tag '" + type->as_string() + "' found using 'type' does not match any of the expected tags");
    }
    try {
        return model::parse<SerialCodeGiveItem>(value);
    } catch (const error::ValidationError &) {
        return model::parse<SerialCodeRunFunction>(value);
    }
}

std::optional<SerialCodeHashed> get_serial_code_hashed(const SerialCode &serial_code) {
    if (!serial_code.serial_code.is_object()) return std::nullopt;
    const Json &j = serial_code.serial_code;
    const Json *salt = j.find("salt"), *hash = j.find("hash");
    if (!salt || !hash || !salt->is_string() || !hash->is_string())
        throw std::invalid_argument("invalid serial code type");
    SerialCodeHashed out;
    out.salt = util::base64_decode(salt->as_string());
    out.hash = hash->as_string();
    return out;
}

// HMAC-SHA256 on top of crypto::sha256 (block size 64).
static Bytes hmac_sha256(std::string_view key, std::string_view message) {
    Bytes k(key);
    if (k.size() > 64) k = crypto::sha256(k);
    k.resize(64, '\0');
    Bytes ipad(64, '\0'), opad(64, '\0');
    for (size_t i = 0; i < 64; i++) {
        ipad[i] = (char)(k[i] ^ 0x36);
        opad[i] = (char)(k[i] ^ 0x5c);
    }
    return crypto::sha256(opad + crypto::sha256(ipad + Bytes(message)));
}

Bytes derive_serial_code_action_key(const std::string &input_code, const Bytes &salt) {
    // PBKDF2(password, salt, dkLen=16, count=4, SHA256): a single block T1.
    Bytes u = hmac_sha256(input_code, salt + std::string("\0\0\0\1", 4));
    Bytes t = u;
    for (int i = 1; i < 4; i++) {
        u = hmac_sha256(input_code, u);
        for (size_t k = 0; k < t.size(); k++) t[k] = (char)(t[k] ^ u[k]);
    }
    t.resize(16);
    return t;
}

Bytes decrypt_action_field(const Bytes &key, const Bytes &salt, const Bytes &data) {
    if (key.size() != 16 || salt.size() < 16) throw std::invalid_argument("invalid key or salt length");
    Bytes nonce = util::xorbytes(salt.substr(0, 8), salt.substr(8));
    unsigned char iv[16] = {0};
    for (size_t i = 0; i < nonce.size() && i < 8; i++) iv[i] = (unsigned char)nonce[i];
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_ctr(), nullptr, (const unsigned char *)key.data(), iv))
        throw std::runtime_error("AES init failed");
    Bytes out(data.size() + 16, '\0');
    int n = 0, m = 0;
    if (!EVP_DecryptUpdate(ctx.get(), (unsigned char *)&out[0], &n, (const unsigned char *)data.data(), (int)data.size()))
        throw std::runtime_error("AES decryption failed");
    if (!EVP_DecryptFinal_ex(ctx.get(), (unsigned char *)&out[0] + n, &m)) throw std::runtime_error("AES decryption failed");
    out.resize(n + m);
    return out;
}

bool check_serial_code(const SerialCode &serial_code, const std::string &input_code_in) {
    std::string input_code = input_code_in;
    if (serial_code.caster) {
        if (*serial_code.caster == "lower") {
            for (char &c : input_code) c = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
        } else if (*serial_code.caster == "upper") {
            for (char &c : input_code) c = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
        }
    }

    if (serial_code.serial_code.is_string()) return serial_code.serial_code.as_string() == input_code;
    auto hashed = get_serial_code_hashed(serial_code);
    if (!hashed) throw std::invalid_argument("invalid serial code type");
    std::string digest = util::hex(crypto::sha256(hashed->salt + input_code));
    std::string expected = hashed->hash;
    for (char &c : expected) c = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    return digest == expected;
}

SerialCodeAction get_action(const SerialCode &serial_code, const std::string &input_code) {
    if (serial_code.action.is_array()) {
        auto hashed = get_serial_code_hashed(serial_code);
        if (!hashed) throw std::invalid_argument("cannot have secure action without secure serial code");

        // Need to decrypt
        Bytes key = derive_serial_code_action_key(input_code, hashed->salt);
        std::string joined;
        for (const Json &part : serial_code.action.elements()) joined += part.as_string();
        Bytes secure_action = decrypt_action_field(key, hashed->salt, util::base64_decode(joined));
        return parse_serial_code_action(Json::parse(secure_action));
    }
    if (serial_code.action.is_object()) return parse_serial_code_action(serial_code.action);
    throw std::invalid_argument("invalid serial code action");
}

}  // namespace npps::data::schema
