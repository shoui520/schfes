#pragma once
// npps4/system/user.py. UserInfoData, NextLevelInfo and UserDiffMixin are generated in app/models.hpp.
#include "app/context.hpp"
#include "app/models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace npps::system::user {

// game_mater.GAME_SETTING (the first game_setting_m row, loaded once).
const db::game_mater::GameSetting &game_setting();

// main.User.set_passwd / check_passwd (npps4/db/main.py)
void set_passwd(db::main::User &user, const std::string &passwd);
bool check_passwd(const db::main::User &user, const std::string &passwd);

// Null when the user does not exist (or id is None).
db::main::User *get(Context &context, std::optional<int64_t> id = std::nullopt);
// Never null; throws std::invalid_argument on a logic error and error::locked() for locked users.
db::main::User *get_current(Context &context);
db::main::User *get_from_token(Context &context, const std::string &token);
UserInfoData get_user_info(Context &context, db::main::User &user);
db::main::User *create(Context &context, const std::optional<std::string> &key,
                       const std::optional<std::string> &passwd);
db::main::User *find_by_key(Context &context, const std::string &key);
// invite_code is compared as an integer like NPPS4 (the column holds a zero-padded string).
db::main::User *find_by_invite_code(Context &context, int64_t invite_code);

int64_t get_current_energy(const db::main::User &user, std::optional<int64_t> t = std::nullopt);
bool has_energy(const db::main::User &user, int64_t energy, std::optional<int64_t> t = std::nullopt);
// Throws std::invalid_argument for a negative amount.
void add_energy(db::main::User &user, int64_t amount, std::optional<int64_t> t = std::nullopt, bool overflow = true);
// Throws std::invalid_argument for a negative amount or not enough energy.
void sub_energy(db::main::User &user, int64_t amount, std::optional<int64_t> t = std::nullopt);
void add_energy_percentage(db::main::User &user, double percentage, int64_t ntimes = 1,
                           std::optional<int64_t> t = std::nullopt, bool overflow = true);
std::vector<NextLevelInfo> add_exp(Context &context, db::main::User &user, int64_t exp);

int64_t get_loveca(const db::main::User &user, bool include_free = true, bool include_paid = true);
bool sub_loveca(db::main::User &user, int64_t amount, bool sub_paid_only = false);

// Throws std::invalid_argument if the user doesn't exist.
void delete_user(Context &context, int64_t user_id);

}  // namespace npps::system::user
