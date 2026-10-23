#pragma once
// npps4/system/handover.py
#include "app/context.hpp"

#include <string>

namespace npps::system::handover {

inline constexpr const char *VALID_CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

std::string generate_passcode_sha1(const std::string &transfer_id, const std::string &transfer_code);

std::string generate_transfer_code();

bool has_passcode_issued(db::main::User &user);

// Null when not found.
db::main::User *find_user_by_passcode(Context &context, const std::string &sha1_code);

void swap_credentials(db::main::User &source_user, db::main::User &target_user);

}  // namespace npps::system::handover
