// npps4/game/friend.py
#include "app/idol.hpp"
#include "system/friend_.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <cctype>
#include <climits>
#include <optional>

namespace npps::game::friend_ {

// int(text) for a str: surrounding whitespace, an optional sign, digits with single underscores between them.
// Out of int64 range values raise OverflowError in the SQLite driver, hence std::overflow_error.
static std::optional<int64_t> parse_int(const std::string &text) {
    size_t begin = 0, end = text.size();
    while (begin < end && std::isspace((unsigned char)text[begin])) begin++;
    while (end > begin && std::isspace((unsigned char)text[end - 1])) end--;
    bool negative = false;
    if (begin < end && (text[begin] == '+' || text[begin] == '-')) negative = text[begin++] == '-';
    if (begin >= end) return std::nullopt;

    // Magnitude saturates past what int64 can hold; overflow is reported like Python's.
    uint64_t magnitude = 0;
    bool overflow = false;
    bool previous_digit = false;
    for (size_t i = begin; i < end; i++) {
        char c = text[i];
        if (c == '_') {
            if (!previous_digit || i + 1 == end) return std::nullopt;
            previous_digit = false;
            continue;
        }
        if (c < '0' || c > '9') return std::nullopt;
        previous_digit = true;
        uint64_t digit = (uint64_t)(c - '0');
        if (magnitude > (UINT64_MAX - digit) / 10) overflow = true;
        else magnitude = magnitude * 10 + digit;
    }
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
    if (overflow || magnitude > limit) throw std::overflow_error("Python int too large to convert to SQLite INTEGER");
    return negative ? (int64_t)(0 - magnitude) : (int64_t)magnitude;
}

static FriendListResponse friend_list(Context &, const FriendListRequest &request) {
    // TODO
    util::log("stub: friend.list type=%lld sort=%lld page=%lld", (long long)request.type, (long long)request.sort,
              (long long)request.page);
    return FriendListResponse::make(0, {}, Json::array());
}
NPPS_ENDPOINT("friend", "list", User, friend_list);

static FriendSearchResponse friend_search(Context &context, const FriendSearchRequest &request) {
    db::main::User *current_user = system::user::get_current(context);

    std::optional<int64_t> invite_code_int = parse_int(request.invite_code);
    if (!invite_code_int) throw error::by_code(error::ERROR_CODE_FRIEND_USER_NOT_EXISTS);

    db::main::User *target_user = system::user::find_by_invite_code(context, *invite_code_int);

    if (target_user == nullptr || target_user->center_unit_owning_user_id == 0)
        throw error::by_code(error::ERROR_CODE_FRIEND_USER_NOT_EXISTS);

    db::main::Unit *unit_data = system::unit::get_unit(context, target_user->center_unit_owning_user_id);
    const db::unit::Unit *unit_info = system::unit::get_unit_info(context, unit_data->unit_id);
    if (unit_info == nullptr) throw std::logic_error("unit_info is not None");
    auto [unit_data_full_info, unit_stats] = system::unit::get_unit_data_full_info(context, *unit_data);

    FriendSearchUserInfo user_info;
    user_info.user_id = target_user->id;
    user_info.name = target_user->name;
    user_info.level = target_user->level;
    // TODO get from game variables
    user_info.cost_max = util::floordiv(target_user->energy_max + target_user->over_max_energy, 25) * 25;
    user_info.unit_max = target_user->unit_max;
    user_info.energy_max = target_user->energy_max;
    user_info.friend_max = target_user->friend_max;
    user_info.unit_cnt = system::unit::count_units(context, *target_user, true);
    user_info.elapsed_time_from_login = "Unknown";
    user_info.comment = target_user->bio;

    FriendSearchUnitInfo center_unit_info;
    const auto &full = unit_data_full_info;
    center_unit_info.unit_owning_user_id = full.unit_owning_user_id;
    center_unit_info.unit_id = full.unit_id;
    center_unit_info.unit_rarity_id = full.unit_rarity_id;
    center_unit_info.exp = full.exp;
    center_unit_info.next_exp = full.next_exp;
    center_unit_info.level = full.level;
    center_unit_info.level_limit_id = full.level_limit_id;
    center_unit_info.max_level = full.max_level;
    center_unit_info.rank = full.rank;
    center_unit_info.max_rank = full.max_rank;
    center_unit_info.love = full.love;
    center_unit_info.max_love = full.max_love;
    center_unit_info.unit_skill_level = full.unit_skill_level;
    center_unit_info.max_hp = full.max_hp;
    center_unit_info.favorite_flag = full.favorite_flag;
    center_unit_info.display_rank = full.display_rank;
    center_unit_info.unit_skill_exp = full.unit_skill_exp;
    center_unit_info.unit_removable_skill_capacity = full.unit_removable_skill_capacity;
    center_unit_info.is_removable_skill_capacity_max = full.is_removable_skill_capacity_max;
    center_unit_info.is_love_max = full.is_love_max;
    center_unit_info.is_level_max = full.is_level_max;
    center_unit_info.is_rank_max = full.is_rank_max;
    center_unit_info.is_signed = full.is_signed;
    center_unit_info.is_skill_level_max = full.is_skill_level_max;
    center_unit_info.attribute = unit_info->attribute_id;
    center_unit_info.smile = unit_stats.smile;
    center_unit_info.cute = unit_stats.pure;
    center_unit_info.cool = unit_stats.cool;
    center_unit_info.setting_award_id = target_user->active_award;
    center_unit_info.removable_skill_ids = system::unit::get_unit_removable_skills(context, *unit_data);

    FriendSearchResponse response;
    response.user_info = user_info;
    response.center_unit_info = center_unit_info;
    response.setting_award_id = target_user->active_award;
    response.is_alliance = false;  // TODO
    response.friend_status = system::friend_::get_friend_status(context, *current_user, *target_user);
    return response;
}
NPPS_ENDPOINT("friend", "search", User, friend_search);

}  // namespace npps::game::friend_
