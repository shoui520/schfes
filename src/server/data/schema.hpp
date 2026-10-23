#pragma once
// npps4/data/schema.py: properties, validators and the serial code action
// encryption helpers. The models themselves are generated in app/models.hpp.
#include "app/models.hpp"

#include <string>
#include <variant>

namespace npps::data::schema {

// HasIDString._internal_id (java_hash_code of id_string). StickerShop is not a
// HasIDString subclass in the generated code, hence the overload.
int64_t _internal_id(const HasIDString &item);
int64_t _internal_id(const StickerShop &item);
// SecretboxData.secretbox_id
inline int64_t secretbox_id(const SecretboxData &secretbox) { return _internal_id(secretbox); }
// StickerShop.exchange_item_id
inline int64_t exchange_item_id(const StickerShop &item) { return _internal_id(item); }

// SerialCodeGiveItem | SerialCodeRunFunction (SERIAL_CODE_ACTION_ADAPTER)
using SerialCodeAction = std::variant<SerialCodeGiveItem, SerialCodeRunFunction>;
// SERIAL_CODE_ACTION_ADAPTER.validate_python / validate_json (discriminated by "type").
SerialCodeAction parse_serial_code_action(const Json &value);

// SerialCode.serial_code is `str | SerialCodeHashed`; held as Json in the
// generated model. Returns the hashed form, or nullopt for a plain string.
// The salt is base64url-decoded (pydantic.Base64UrlBytes).
std::optional<SerialCodeHashed> get_serial_code_hashed(const SerialCode &serial_code);

// PBKDF2-HMAC-SHA256, 16 bytes, 4 iterations.
Bytes derive_serial_code_action_key(const std::string &input_code, const Bytes &salt);
// initialize_aes_for_action_field(key, salt).decrypt(data): AES-CTR, nonce = salt[:8] ^ salt[8:], counter 0.
Bytes decrypt_action_field(const Bytes &key, const Bytes &salt, const Bytes &data);

// SerialCode.check_serial_code
bool check_serial_code(const SerialCode &serial_code, const std::string &input_code);
// SerialCode.get_action; throws std::invalid_argument (ValueError).
SerialCodeAction get_action(const SerialCode &serial_code, const std::string &input_code);

}  // namespace npps::data::schema
