// npps4/system/profile.py
#include "system/profile.hpp"

#include "system/museum.hpp"
#include "system/unit.hpp"

#include <stdexcept>

namespace npps::system::profile {

ProfileUnitInfo to_profile_unit_info(Context &context, const db::main::Unit &unit_data,
                                     const museum::MuseumParameterData &museum_param) {
    const db::unit::Unit *unit_info = unit::get_unit_info(context, unit_data.unit_id);

    auto [unit_full_data, unit_stats] = unit::get_unit_data_full_info(context, unit_data);
    // Calculate unit level
    const db::unit::Rarity *unit_rarity = unit::get_unit_rarity(context, unit_info->rarity);
    if (!unit_rarity) throw std::runtime_error("unit_rarity is none");

    bool idolized = unit_data.rank == unit_info->rank_max;
    int64_t real_max_exp = unit_stats.level == unit_rarity->before_level_max && !idolized ? 0 : unit_stats.next_exp;
    bool removable_skill_max = unit_data.unit_removable_skill_capacity == unit_info->max_removable_skill_capacity;

    ProfileUnitInfo info;
    info.unit_owning_user_id = unit_data.id;
    info.unit_id = unit_data.unit_id;
    info.exp = unit_full_data.exp;
    info.next_exp = real_max_exp;
    info.level = unit_full_data.level;
    info.level_limit_id = unit_data.level_limit_id;
    info.max_level = unit_data.max_level;
    info.rank = unit_data.rank;
    info.max_rank = unit_full_data.max_rank;
    info.love = unit_data.love;
    info.max_love = unit_full_data.max_love;
    info.unit_skill_level = unit_full_data.unit_skill_level;
    info.max_hp = unit_full_data.max_hp;
    info.favorite_flag = unit_data.favorite_flag;
    info.display_rank = unit_data.display_rank;
    info.unit_skill_exp = unit_data.skill_exp;
    info.unit_removable_skill_capacity = unit_data.unit_removable_skill_capacity;
    info.total_smile = unit_stats.smile + museum_param.smile;
    info.total_cute = unit_stats.pure + museum_param.pure;
    info.total_cool = unit_stats.cool + museum_param.cool;
    info.total_hp = unit_full_data.max_hp;
    info.is_love_max = unit_full_data.is_love_max;
    info.is_level_max = unit_full_data.is_level_max;
    info.is_rank_max = unit_full_data.is_rank_max;
    info.is_signed = unit_data.is_signed;
    info.is_skill_level_max = unit_full_data.is_skill_level_max;
    info.is_removable_skill_capacity_max = removable_skill_max;
    info.removable_skill_ids = {};
    info.insert_date = util::timestamp_to_datetime(unit_data.insert_date);
    return info;
}

}  // namespace npps::system::profile
