// npps4/system/user.py
#include "system/user.hpp"

#include "app/session.hpp"
#include "core/crypto.hpp"
#include "system/achievement.hpp"
#include "system/award.hpp"
#include "system/background.hpp"
#include "system/core.hpp"
#include "system/item.hpp"
#include "system/live.hpp"
#include "system/scenario.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace npps::system::user {

static constexpr size_t SALT_SIZE = 16;

const db::game_mater::GameSetting &game_setting() {
    static const db::game_mater::GameSetting *setting = nullptr;
    if (setting == nullptr) {
        setting = databases().game_mater.first<db::game_mater::GameSetting>("");
        if (setting == nullptr) throw std::runtime_error("unable to load client setting");
    }
    return *setting;
}

void set_passwd(db::main::User &user, const std::string &passwd) {
    Bytes salt = util::randbytes(SALT_SIZE);
    Bytes hmac_hash = crypto::hmac_sha512(passwd, salt);
    Bytes result = salt + hmac_hash.substr(SALT_SIZE);
    user.passwd = util::base64_encode(result);
}

bool check_passwd(const db::main::User &user, const std::string &passwd) {
    if (!user.passwd) return false;
    Bytes result = util::base64_decode(*user.passwd);
    Bytes salt = result.substr(0, SALT_SIZE);
    Bytes hmac_hash = crypto::hmac_sha512(passwd, salt);
    return result.substr(SALT_SIZE) == hmac_hash.substr(SALT_SIZE);
}

db::main::User *get(Context &context, std::optional<int64_t> id) {
    if (!id) return nullptr;
    return context.db.main.get<db::main::User>(*id);
}

db::main::User *get_current(Context &context) {
    if (!context.token) throw std::invalid_argument("logic error, token is None");

    db::main::User *result = context.db.main.get<db::main::User>(context.token->user_id);
    if (result == nullptr) throw std::invalid_argument("logic error, user is None");

    context.add_task([] { session::try_cleanup_tokens(); });

    if (result->locked) throw error::locked();

    return result;
}

db::main::User *get_from_token(Context &context, const std::string &token) {
    std::optional<TokenData> detoken = session::decapsulate_token(context, token);
    if (detoken) return context.db.main.get<db::main::User>(detoken->user_id);
    return nullptr;
}

UserInfoData get_user_info(Context &context, db::main::User &user) {
    UserInfoData info;
    info.user_id = user.id;
    info.name = user.name;
    info.level = user.level;
    info.exp = user.exp;
    info.previous_exp = core::get_next_exp_cumulative(user.level - 1);
    info.next_exp = core::get_next_exp_cumulative(user.level);
    info.game_coin = user.game_coin;
    info.sns_coin = user.free_sns_coin + user.paid_sns_coin;
    info.free_sns_coin = user.free_sns_coin;
    info.paid_sns_coin = user.paid_sns_coin;
    info.social_point = user.social_point;
    info.unit_max = user.unit_max;
    info.waiting_unit_max = user.waiting_unit_max;
    info.current_energy = get_current_energy(user);
    info.energy_max = user.energy_max;
    info.energy_full_time = util::timestamp_to_datetime(user.energy_full_time);
    info.license_live_energy_recoverly_time = user.license_live_energy_recoverly_time;
    info.energy_full_need_time =
        (user.over_max_energy == 0) * std::max<int64_t>(user.energy_full_time - util::time(), 0);
    info.over_max_energy = user.over_max_energy;
    info.training_energy = user.training_energy;
    info.training_energy_max = user.training_energy_max;
    info.friend_max = user.friend_max;
    info.invite_code = user.invite_code;
    info.insert_date = util::timestamp_to_datetime(user.insert_date);
    info.update_date = util::timestamp_to_datetime(user.update_date);
    info.tutorial_state = user.tutorial_state;
    info.lp_recovery_item = item::get_recovery_items(context, user);
    return info;
}

db::main::User *create(Context &context, const std::optional<std::string> &key,
                       const std::optional<std::string> &passwd) {
    db::main::User new_user;
    new_user.key = key;
    if (passwd) set_passwd(new_user, *passwd);
    db::main::User *user = context.db.main.add(new_user);
    context.db.main.flush();
    user->invite_code = util::format("%09lld", (long long)core::get_invite_code(user->id));
    achievement::init(context, *user);
    background::init(context, *user);
    award::init(context, *user);
    live::init(context, *user);
    scenario::init(context, *user);
    context.db.main.flush();
    return user;
}

db::main::User *find_by_key(Context &context, const std::string &key) {
    return context.db.main.first<db::main::User>("WHERE \"key\" = ?", key);
}

db::main::User *find_by_invite_code(Context &context, int64_t invite_code) {
    return context.db.main.first<db::main::User>("WHERE \"invite_code\" = ?", invite_code);
}

int64_t get_current_energy(const db::main::User &user, std::optional<int64_t> t) {
    int64_t now = t ? *t : util::time();
    int64_t difftime = std::max<int64_t>(user.energy_full_time - now, 0);
    int64_t used_lp = (int64_t)std::ceil((double)difftime / (double)game_setting().live_energy_recoverly_time);
    return std::max<int64_t>(user.energy_max - used_lp, 0) + std::max<int64_t>(user.over_max_energy - user.energy_max, 0);
}

bool has_energy(const db::main::User &user, int64_t energy, std::optional<int64_t> t) {
    return get_current_energy(user, t) >= energy;
}

void add_energy(db::main::User &user, int64_t amount, std::optional<int64_t> t, bool overflow) {
    if (amount < 0) throw std::invalid_argument("to subtract use sub_energy");

    int64_t now = t ? *t : util::time();

    int64_t before = get_current_energy(user, now);
    int64_t sub_time = amount * game_setting().live_energy_recoverly_time;
    user.energy_full_time = std::max<int64_t>(user.energy_full_time - sub_time, now);

    if (overflow) {
        int64_t current_energy = get_current_energy(user, now);
        amount = amount - (current_energy - before);
        if (amount > 0) user.over_max_energy = std::max(user.over_max_energy, user.energy_max) + amount;
        else user.over_max_energy = 0;
    }
}

void sub_energy(db::main::User &user, int64_t amount, std::optional<int64_t> t) {
    if (amount < 0) throw std::invalid_argument("to add use add_energy");

    int64_t now = t ? *t : util::time();

    int64_t current_energy = get_current_energy(user, now);
    int64_t overflow_energy = 0;
    if (user.over_max_energy > 0) {
        overflow_energy = user.over_max_energy - user.energy_max;
        current_energy = user.energy_max;
    }

    // Is overflow LP >= LP?
    if (overflow_energy >= amount) {
        user.over_max_energy = user.over_max_energy - amount;
        return;
    }

    // Overflow LP < LP.
    int64_t consume_normal_energy = amount - overflow_energy;
    if (current_energy >= consume_normal_energy) {
        user.energy_full_time = std::max(user.energy_full_time, now) +
                                game_setting().live_energy_recoverly_time * consume_normal_energy;
        user.over_max_energy = 0;
        return;
    }

    throw std::invalid_argument("not enough energy");
}

void add_energy_percentage(db::main::User &user, double percentage, int64_t ntimes, std::optional<int64_t> t,
                           bool overflow) {
    if (percentage < 0.0 || percentage > 1.0) throw std::invalid_argument("percentage out of range");
    int64_t now = t ? *t : util::time();

    int64_t energy_to_add = (int64_t)std::ceil((double)user.energy_max * percentage) * ntimes;
    add_energy(user, energy_to_add, now, overflow);
}

std::vector<NextLevelInfo> add_exp(Context &context, db::main::User &user, int64_t exp) {
    int64_t next_exp = core::get_next_exp_cumulative(user.level);
    bool level_up = false;
    int64_t max_energy = 0;
    std::vector<NextLevelInfo> next_level_info = {NextLevelInfo::make(user.level, user.exp)};
    int64_t t = util::time();

    user.exp = user.exp + exp;
    while (user.exp >= next_exp) {
        level_up = true;
        user.level = user.level + 1;
        next_level_info.push_back(NextLevelInfo::make(user.level, next_exp));
        next_exp = core::get_next_exp_cumulative(user.level);
        max_energy = core::get_energy_by_rank(user.level);
        // Try to increase max LP. Do it here so add_energy below consider it into account.
        user.energy_max = max_energy;
        add_energy(user, max_energy, t);
    }

    if (level_up) {
        // Increase max friend
        user.friend_max = core::get_max_friend_by_rank(user.level);
        // Increase master ticket
        user.training_energy_max = core::get_training_energy_by_rank(user.level);
    }

    context.db.main.flush();
    return next_level_info;
}

int64_t get_loveca(const db::main::User &user, bool include_free, bool include_paid) {
    return user.free_sns_coin * include_free + user.paid_sns_coin * include_paid;
}

bool sub_loveca(db::main::User &user, int64_t amount, bool sub_paid_only) {
    if (sub_paid_only) {
        if (user.paid_sns_coin < amount) return false;

        user.paid_sns_coin = user.paid_sns_coin - amount;
        return true;
    }

    if (get_loveca(user) < amount) return false;

    int64_t amount1 = std::min(user.free_sns_coin, amount);
    int64_t amount2 = amount - amount1;
    user.free_sns_coin = user.free_sns_coin - amount1;
    user.paid_sns_coin = user.paid_sns_coin - amount2;
    return true;
}

template <typename T> static void clean_table(Context &context, int64_t user_id) {
    context.db.main.execute(std::string("DELETE FROM \"") + T::TABLE + "\" WHERE \"user_id\" = ?", user_id);
}

void delete_user(Context &context, int64_t user_id) {
    db::main::User *user_data = get(context, user_id);
    if (user_data == nullptr) throw std::invalid_argument("User doesn't exist");
    clean_table<db::main::NormalLiveUnlock>(context, user_id);
    clean_table<db::main::LocalSerialCodeUsage>(context, user_id);
    clean_table<db::main::ExchangePointItem>(context, user_id);
    clean_table<db::main::RecoveryItem>(context, user_id);
    clean_table<db::main::Item>(context, user_id);
    clean_table<db::main::LiveInProgress>(context, user_id);
    clean_table<db::main::UnitRemovableSkill>(context, user_id);
    clean_table<db::main::RemovableSkillInfo>(context, user_id);
    clean_table<db::main::MuseumUnlock>(context, user_id);
    clean_table<db::main::LiveClear>(context, user_id);
    clean_table<db::main::SubScenario>(context, user_id);
    clean_table<db::main::Scenario>(context, user_id);
    clean_table<db::main::Incentive>(context, user_id);
    clean_table<db::main::LoginBonus>(context, user_id);
    clean_table<db::main::Achievement>(context, user_id);
    clean_table<db::main::Album>(context, user_id);
    clean_table<db::main::UnitSupporter>(context, user_id);
    clean_table<db::main::UnitDeck>(context, user_id);
    clean_table<db::main::TOSAgree>(context, user_id);
    clean_table<db::main::Award>(context, user_id);
    clean_table<db::main::Background>(context, user_id);
    clean_table<db::main::RequestCache>(context, user_id);

    // Delete session
    clean_table<db::main::Session>(context, user_id);

    // Perform failsafe on party_user_id
    context.db.main.execute("UPDATE \"live_in_progress\" SET \"party_user_id\" = \"user_id\" WHERE \"party_user_id\" = ?",
                            user_id);

    clean_table<db::main::Unit>(context, user_id);

    // Delete user
    context.db.main.remove(user_data);
    context.db.main.flush();
}

}  // namespace npps::system::user
