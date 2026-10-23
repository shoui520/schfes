#pragma once
// npps4/system/lila.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace npps::system::lila {

class BadSignature : public std::runtime_error {
public:
    BadSignature() : std::runtime_error("account data bad signature") {}
};

// CommonItemData.tuple / CommonItemData.has_quantity
inline std::tuple<int64_t, int64_t> tuple(const CommonItemData &d) { return {d.id, d.amount}; }
inline bool has_quantity(const CommonItemData &d) { return d.amount > 0; }

// Returns (16-byte salt + zlib-compressed JSON, HMAC-SHA256(salt + uncompressed JSON) keyed by secret_key).
// secret_key None -> config secret key.
std::tuple<Bytes, Bytes> export_user(Context &context, db::main::User &target,
                                     const std::optional<Bytes> &secret_key = std::nullopt,
                                     bool nullify_credentials = false);

std::tuple<Bytes, Bytes> export_account_data(const AccountData &account_data,
                                             const std::optional<Bytes> &secret_key = std::nullopt);

// Throws BadSignature when `signature` is given and does not match.
AccountData extract_serialized_data(const Bytes &serialized_data, const std::optional<Bytes> &signature,
                                    const std::optional<Bytes> &secret_key = std::nullopt);

db::main::User *import_user(Context &context, const AccountData &serialized_data);

}  // namespace npps::system::lila
