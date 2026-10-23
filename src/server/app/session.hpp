#pragma once
// npps4/idol/session.py token handling.
#include "app/context.hpp"

namespace npps::session {

constexpr int64_t FIRST_STAGE_TOKEN_MAX_DURATION = 60;

std::string encapsulate_token(Context &context, const Bytes &server_key, const Bytes &client_key, int64_t user_id = 0);
std::optional<TokenData> decapsulate_token(Context &context, const std::string &token_data);
void invalidate_current(Context &context);
void cleanup_session_table(Context &context);
// Background task: cleans the session table at most every few seconds.
void try_cleanup_tokens();

}  // namespace npps::session
