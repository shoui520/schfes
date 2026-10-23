#include "session.hpp"

#include "app/config.hpp"
#include "core/crypto.hpp"

namespace npps::session {

static constexpr size_t SALT_SIZE = 16, TOKEN_SIZE = 16, SIGNATURE_SIZE = 20;

// Opaque to the client: base64url(salt + token + HMAC-SHA1(salt + token)).
// The database stores only the token, like NPPS4.
static Bytes sign(std::string_view salt, std::string_view token) {
    return crypto::hmac_sha1(std::string(salt) + std::string(token), config::get().secret_key);
}

std::string encapsulate_token(Context &context, const Bytes &server_key, const Bytes &client_key, int64_t user_id) {
    Bytes salt = util::randbytes(SALT_SIZE);
    std::string token = util::hex(util::randbytes(TOKEN_SIZE)).substr(0, TOKEN_SIZE);
    db::main::Session row;
    row.token = token;
    if (user_id != 0) row.user_id = user_id;
    row.client_key = client_key;
    row.server_key = server_key;
    context.db.main.add(std::move(row));
    context.db.main.flush();
    return util::base64_urlsafe_encode(salt + token + sign(salt, token));
}

std::optional<TokenData> decapsulate_token(Context &context, const std::string &token_data) {
    Bytes raw;
    try {
        raw = util::base64_decode(token_data);
    } catch (const std::exception &) {
        return std::nullopt;
    }
    if (raw.size() != SALT_SIZE + TOKEN_SIZE + SIGNATURE_SIZE) return std::nullopt;
    std::string_view salt(raw.data(), SALT_SIZE), token(raw.data() + SALT_SIZE, TOKEN_SIZE);
    if (!crypto::constant_time_equal(sign(salt, token), std::string_view(raw).substr(SALT_SIZE + TOKEN_SIZE)))
        return std::nullopt;

    int64_t expiry = config::get_session_expiry_time();
    db::main::Session *row;
    if (expiry > 0)
        row = context.db.main.first<db::main::Session>("WHERE token = ? AND last_accessed >= ?", std::string(token),
                                                       util::time() - expiry);
    else
        row = context.db.main.first<db::main::Session>("WHERE token = ?", std::string(token));
    if (!row) return std::nullopt;
    row->last_accessed = util::time();
    return TokenData{row->client_key, row->server_key, row->user_id.value_or(0)};
}

void invalidate_current(Context &context) {
    // NPPS4 compares the encapsulated text with the stored token, which never
    // matches; kept as is so behaviour is identical.
    if (context.token_text) context.db.main.execute("DELETE FROM session WHERE token = ?", *context.token_text);
}

void cleanup_session_table(Context &context) {
    int64_t t = util::time();
    context.db.main.execute("DELETE FROM session WHERE user_id IS NULL AND last_accessed < ?",
                            t - FIRST_STAGE_TOKEN_MAX_DURATION);
    int64_t expiry = config::get_session_expiry_time();
    if (expiry > 0) context.db.main.execute("DELETE FROM session WHERE last_accessed < ?", t - expiry);
}

void try_cleanup_tokens() {
    static uint64_t last_us = 0;
    uint64_t now = util::monotonic_us();
    if (last_us != 0 && now - last_us < 5000000) return;
    last_us = now;
    Context context;
    try {
        cleanup_session_table(context);
        context.commit();
    } catch (...) {
        context.rollback();
        throw;
    }
}

}  // namespace npps::session
