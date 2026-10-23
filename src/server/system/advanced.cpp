// npps4/system/advanced.py
#include "system/advanced.hpp"

#include "external/badwords.hpp"
#include "system/leader_skill.hpp"
#include "system/award.hpp"
#include "system/background.hpp"
#include "system/exchange.hpp"
#include "system/item.hpp"
#include "system/item_model.hpp"
#include "system/live.hpp"
#include "system/museum.hpp"
#include "system/removable_skill.hpp"
#include "system/scenario.hpp"
#include "system/unit.hpp"

#include <functional>
#include <map>

namespace npps::system::advanced {

namespace {

// util.copy_attr(src, dst) where dst is a plain Item: fields shared with Item are assigned, the others are
// stored as extra attributes.
void copy_attr(const model::Model &src, item_model::Item &dst) {
    Json dumped = src.to_json();
    for (const auto &m : dumped.members()) {
        const std::string &k = m.first;
        if (k == "add_type") dst.add_type = (const_::ADD_TYPE)m.second.as_int();
        else if (k == "item_id") dst.item_id = m.second.as_int();
        else if (k == "amount") dst.amount = m.second.as_int();
        else if (k == "item_category_id") dst.item_category_id = m.second.as_int();
        else if (k == "reward_box_flag") dst.reward_box_flag = m.second.as_bool();
        else if (k == "comment") dst.comment = m.second.as_string();
        else dst.extra[k] = m.second;
    }
}

bool is_space_code_point(uint32_t c) {
    // str.isspace(): characters with bidirectional type WS, B or S, or category Zs.
    if (c >= 9 && c <= 13) return true;
    if (c >= 0x1c && c <= 0x20) return true;
    switch (c) {
    case 0x85: case 0xa0: case 0x1680: case 0x2028: case 0x2029: case 0x202f: case 0x205f: case 0x3000: return true;
    default: return c >= 0x2000 && c <= 0x200a;
    }
}

// str.isspace() on UTF-8 text.
bool is_space_string(const std::string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char b = (unsigned char)s[i];
        uint32_t c;
        size_t n;
        if (b < 0x80) { c = b; n = 1; }
        else if ((b >> 5) == 6) { c = b & 0x1f; n = 2; }
        else if ((b >> 4) == 14) { c = b & 0x0f; n = 3; }
        else { c = b & 0x07; n = 4; }
        for (size_t k = 1; k < n && i + k < s.size(); k++) c = (c << 6) | ((unsigned char)s[i + k] & 0x3f);
        if (!is_space_code_point(c)) return false;
        i += n;
    }
    return true;
}

}  // namespace

AddResult add_item(Context &context, db::main::User &user, const model::Ptr<item_model::Item> &i) {
    switch (i->add_type) {
    case const_::ADD_TYPE::ITEM:
        switch (i->item_id) {
        case 2:
            user.social_point = user.social_point + i->amount;
            return {true};
        case 3:
            user.game_coin = user.game_coin + i->amount;
            return {true};
        case 4:
            user.free_sns_coin = user.free_sns_coin + i->amount;
            return {true};
        default:
            item::add_item(context, user, i->item_id, i->amount);
            return {true};
        }
    case const_::ADD_TYPE::UNIT:
        if (unit::is_support_member(context, i->item_id)) {
            return {unit::add_supporter_unit(context, user, i->item_id, i->amount)};
        } else {
            int64_t unit_cnt = unit::count_units(context, user, true);
            if ((unit_cnt + i->amount) < user.unit_max) {
                if (typeid(*i) == typeid(unit_model::UnitSupportItem)) throw std::logic_error("assertion failed");

                auto i_unit = std::dynamic_pointer_cast<unit_model::UnitItem>(i);
                for (int64_t n = 0; n < i->amount; n++) {
                    model::Ptr<unit_model::UnitItem> unit_item;
                    if (i_unit) {
                        unit_item = i_unit;
                    } else {
                        unit_item = std::dynamic_pointer_cast<unit_model::UnitItem>(
                            unit::create_unit_item(context, i->item_id));
                        if (!unit_item) throw std::logic_error("assertion failed");
                    }

                    db::main::Unit unit_data = unit::create_unit_data(context, user, *unit_item, true);
                    db::main::Unit *added = unit::add_unit_by_object(context, user, unit_data);

                    unit_item->unit_owning_user_id = added->id;
                    if (!i_unit) copy_attr(*unit_item, *i);
                }

                return {true};
            } else {
                return {false};
            }
        }
    case const_::ADD_TYPE::GAME_COIN:
        user.game_coin = user.game_coin + i->amount;
        return {true};
    case const_::ADD_TYPE::LOVECA:
        if (i->item_id == 1) user.paid_sns_coin = user.paid_sns_coin + i->amount;
        else user.free_sns_coin = user.free_sns_coin + i->amount;
        return {true};
    case const_::ADD_TYPE::SOCIAL_POINT:
        user.social_point = user.social_point + i->amount;
        return {true};
    case const_::ADD_TYPE::EXCHANGE_POINT:
        return {exchange::add_exchange_point(context, user, i->item_id, i->amount)};
    case const_::ADD_TYPE::UNIT_MAX:
        // TODO: Limit max units
        user.unit_max = user.unit_max + i->amount;
        return {true};
    // FIXME: Actually check for their return values of these unlocks.
    case const_::ADD_TYPE::LIVE: {
        bool success = live::unlock_normal_live(context, user, i->item_id);
        if (success) {
            auto normal = live::get_normal_live_clear_status_of_track(context, user, i->item_id);
            auto training = live::get_training_live_clear_status_of_track(context, user, i->item_id);
            if (auto live_item = std::dynamic_pointer_cast<live_model::LiveItem>(i)) {
                live_item->additional_normal_live_status_list = std::move(normal);
                live_item->additional_training_live_status_list = std::move(training);
            } else {
                i->extra["additional_normal_live_status_list"] = model::dump(normal);
                i->extra["additional_training_live_status_list"] = model::dump(training);
            }
        }
        return {success};
    }
    case const_::ADD_TYPE::AWARD:
        return {award::unlock_award(context, user, i->item_id)};
    case const_::ADD_TYPE::BACKGROUND:
        return {background::unlock_background(context, user, i->item_id)};
    case const_::ADD_TYPE::SCENARIO:
        return {scenario::unlock(context, user, i->item_id)};
    case const_::ADD_TYPE::SCHOOL_IDOL_SKILL:
        unit::add_unit_removable_skill(context, user, i->item_id, i->amount);
        return {true};
    case const_::ADD_TYPE::RECOVER_LP_ITEM:
        item::add_recovery_item(context, user, i->item_id, i->amount);
        return {true};
    case const_::ADD_TYPE::MUSEUM:
        return {museum::unlock(context, user, i->item_id)};
    default: break;
    }
    return {false};  // TODO
}

PartyInfo get_user_guest_party_info(Context &context, db::main::User &user) {
    PartyUserInfo party_user_info = PartyUserInfo::make(user.id, user.name, user.level);

    // Get unit center info
    int64_t unit_center = unit::get_unit_center(context, user);

    db::main::Unit *unit_data = unit::get_unit(context, unit_center);
    unit::validate_unit(user, unit_data);
    const db::unit::Unit *unit_info = unit::get_unit_info(context, unit_data->unit_id);
    if (unit_info == nullptr) throw std::invalid_argument("invalid user center no info");

    auto [unit_full_data, unit_stats] = unit::get_unit_data_full_info(context, *unit_data);
    PartyCenterUnitInfo party_unit_info;
    party_unit_info.unit_owning_user_id = unit_data->id;
    party_unit_info.unit_id = unit_data->unit_id;
    party_unit_info.exp = unit_full_data.exp;
    party_unit_info.next_exp = unit_full_data.next_exp;
    party_unit_info.level = unit_full_data.level;
    party_unit_info.level_limit_id = unit_data->level_limit_id;
    party_unit_info.max_level = unit_data->max_level;
    party_unit_info.rank = unit_data->rank;
    party_unit_info.max_rank = unit_full_data.max_rank;
    party_unit_info.love = unit_data->love;
    party_unit_info.max_love = unit_full_data.max_love;
    party_unit_info.unit_skill_level = unit_full_data.unit_skill_level;
    party_unit_info.max_hp = unit_full_data.max_hp;
    party_unit_info.favorite_flag = unit_data->favorite_flag;
    party_unit_info.display_rank = unit_data->display_rank;
    party_unit_info.unit_skill_exp = unit_data->skill_exp;
    party_unit_info.unit_removable_skill_capacity = unit_data->unit_removable_skill_capacity;
    party_unit_info.attribute = unit_info->attribute_id;
    party_unit_info.smile = unit_stats.smile;
    party_unit_info.cute = unit_stats.pure;
    party_unit_info.cool = unit_stats.cool;
    party_unit_info.is_love_max = unit_full_data.is_love_max;
    party_unit_info.is_level_max = unit_full_data.is_level_max;
    party_unit_info.is_rank_max = unit_full_data.is_rank_max;
    party_unit_info.is_signed = unit_data->is_signed;
    party_unit_info.is_skill_level_max = unit_full_data.is_skill_level_max;
    party_unit_info.setting_award_id = user.active_award;

    PartyInfo info;
    info.user_info = party_user_info;
    info.center_unit_info = party_unit_info;
    info.setting_award_id = user.active_award;
    // TODO
    info.available_social_point = 5;
    info.friend_status = 0;
    return info;
}

std::vector<db::main::User *> get_random_user_for_partylist(Context &context, db::main::User &user,
                                                            bool include_current, int64_t limit) {
    std::vector<db::main::User *> userlist = context.db.main.select<db::main::User>(
        "WHERE \"tutorial_state\" = -1 AND \"id\" != ? ORDER BY RANDOM() LIMIT ?", user.id, limit);
    if (include_current) userlist.insert(userlist.begin(), &user);
    return userlist;
}

void test_name(Context &context, const std::string &name) {
    if (is_space_string(name)) throw error::by_code(error::ERROR_CODE_ONLY_WHITESPACE_CHARACTERS);
    for (unsigned char c : name)
        if (c < 32) throw error::by_code(error::ERROR_CODE_UNAVAILABLE_WORDS);
    if (external::badwords::has_badwords(context, name)) throw error::by_code(error::ERROR_CODE_NG_WORDS);
}

namespace {

using RewardList = std::vector<model::Ptr<item_model::Item>>;

// _ACHIEVEMENT_REWARD_REPLACE_CRITERIA
const std::map<const_::ADD_TYPE, std::function<bool(Context &, db::main::User &, int64_t)>> &replace_criteria() {
    static const std::map<const_::ADD_TYPE, std::function<bool(Context &, db::main::User &, int64_t)>> criteria = {
        {const_::ADD_TYPE::LIVE, live::has_normal_live_unlock},
        {const_::ADD_TYPE::AWARD, award::has_award},
        {const_::ADD_TYPE::BACKGROUND, background::has_background},
        {const_::ADD_TYPE::SCENARIO, scenario::is_unlocked},
        {const_::ADD_TYPE::MUSEUM, museum::has},
    };
    return criteria;
}

}  // namespace

// Certain reward can only be given once. This function replaces them to give 1 loveca instead.
std::vector<std::vector<model::Ptr<item_model::Item>>> fixup_achievement_reward(
    Context &context, db::main::User &user, const std::vector<std::vector<model::Ptr<item_model::Item>>> &rewardss) {
    std::vector<RewardList> result;

    for (const RewardList &reward_list : rewardss) {
        RewardList new_reward_list;

        for (const auto &ach_reward : reward_list) {
            auto it = replace_criteria().find(ach_reward->add_type);
            if (it != replace_criteria().end() && it->second(context, user, ach_reward->item_id)) {
                new_reward_list.push_back(item::loveca(1));
            } else {
                new_reward_list.push_back(ach_reward);
            }
        }

        result.push_back(std::move(new_reward_list));
    }

    return result;
}

std::tuple<std::vector<LiveDeckStats>, std::vector<unit_model::UnitInfoData>> TeamStatCalculator::get_base_team_stats(
    const std::vector<db::main::Unit *> &player_units) {
    std::vector<LiveDeckStats> result;
    std::vector<unit_model::UnitInfoData> unit_full_data_list;

    // Retrieve base stats
    for (db::main::Unit *unit_data : player_units) {
        auto [unit_full_data, stats] = unit::get_unit_data_full_info(context, *unit_data);
        LiveDeckStats s;
        s.smile = stats.smile;
        s.cute = stats.pure;
        s.cool = stats.cool;
        s.hp = stats.hp;
        result.push_back(s);
        unit_full_data_list.push_back(std::move(unit_full_data));
    }

    return {std::move(result), std::move(unit_full_data_list)};
}

std::vector<LiveDeckStats> TeamStatCalculator::apply_bond_to_stat(const std::vector<db::main::Unit *> &player_units) {
    std::vector<LiveDeckStats> result;

    for (db::main::Unit *unit_data : player_units) {
        const db::unit::Unit *unit_info = unit::get_unit_info(context, unit_data->unit_id);
        int64_t smile = 0, pure = 0, cool = 0;

        switch (unit_info->attribute_id) {
        case 1: smile = unit_data->love; break;
        case 2: pure = unit_data->love; break;
        case 3: cool = unit_data->love; break;
        default: break;
        }

        LiveDeckStats s;
        s.smile = smile;
        s.cute = pure;
        s.cool = cool;
        result.push_back(s);
    }

    return result;
}

std::vector<LiveDeckStats> TeamStatCalculator::apply_museum_stats(const museum::MuseumParameterData &museum_param) {
    LiveDeckStats s;
    s.smile = museum_param.smile;
    s.cute = museum_param.pure;
    s.cool = museum_param.cool;
    s.hp = 0;
    return std::vector<LiveDeckStats>(9, s);
}

std::vector<LiveDeckStats> TeamStatCalculator::apply_sis_stats(const std::vector<db::main::Unit *> &player_units,
                                                               const std::vector<LiveDeckStats> &stats) {
    std::vector<LiveDeckStats> result(stats.size());

    for (size_t i = 0; i < player_units.size(); i++) {
        db::main::Unit *unit_data = player_units[i];
        const db::unit::Unit *unit_info = get_unit_info(unit_data->unit_id);

        for (int64_t sis_id : unit::get_unit_removable_skills(context, *unit_data)) {
            const db::unit::RemovableSkill *sis_info = unit::get_removable_skill_game_info(context, sis_id);

            // Ignore certain SIS
            if (sis_info == nullptr) continue;
            if (sis_info->skill_type != 1) continue;
            if (sis_info->effect_type != 1 && sis_info->effect_type != 2 && sis_info->effect_type != 3) continue;

            if (!removable_skill::can_apply_to_unit(context, *sis_info, *unit_info)) continue;

            if (!removable_skill::can_apply(context, sis_info->trigger_reference_type, sis_info->trigger_type,
                                            player_units))
                continue;

            switch (sis_info->effect_range) {
            case 1: {
                // Apply to self
                const LiveDeckStats &base_stat = stats.at(i);
                LiveDeckStats &dest_stat = result.at(i);
                auto [smile, pure, cool] =
                    removable_skill::apply_stats(sis_info->effect_type, sis_info->effect_value,
                                                 sis_info->fixed_value_flag > 0, base_stat.smile, base_stat.cute,
                                                 base_stat.cool);
                dest_stat.smile = dest_stat.smile + smile;
                dest_stat.cute = dest_stat.cute + pure;
                dest_stat.cool = dest_stat.cool + cool;
                break;
            }
            case 2: {
                // Apply to all team
                for (size_t k = 0; k < result.size() && k < stats.size(); k++) {
                    LiveDeckStats &dest_stat = result[k];
                    const LiveDeckStats &base_stat = stats[k];
                    auto [smile, pure, cool] =
                        removable_skill::apply_stats(sis_info->effect_type, sis_info->effect_value,
                                                     sis_info->fixed_value_flag > 0, base_stat.smile, base_stat.cute,
                                                     base_stat.cool);
                    dest_stat.smile = dest_stat.smile + smile;
                    dest_stat.cute = dest_stat.cute + pure;
                    dest_stat.cool = dest_stat.cool + cool;
                }
                break;
            }
            default: break;
            }
        }
    }

    return result;
}

std::vector<LiveDeckStats> TeamStatCalculator::apply_leader_stats(const std::vector<db::main::Unit *> &player_units,
                                                                  const std::vector<LiveDeckStats> &stats,
                                                                  db::main::Unit &leader_unit) {
    if (stats.size() != player_units.size()) throw std::invalid_argument("stats and player_units are different");

    std::vector<LiveDeckStats> result(stats.size());

    const db::unit::Unit *center = get_unit_info(leader_unit.unit_id);

    if (center->default_leader_skill_id && *center->default_leader_skill_id != 0) {
        const db::unit::LeaderSkill *leader_skill_data = get_unit_leader_skill(*center->default_leader_skill_id);

        // Apply leader skill
        for (size_t k = 0; k < result.size(); k++) {
            LiveDeckStats &stat = result[k];
            const LiveDeckStats &base_stat = stats[k];
            auto [smile, pure, cool] = leader_skill::calculate_bonus(
                leader_skill_data->leader_skill_effect_type, leader_skill_data->effect_value, base_stat.smile,
                base_stat.cute, base_stat.cool);
            stat.smile = stat.smile + smile;
            stat.cute = stat.cute + pure;
            stat.cool = stat.cool + cool;
        }

        // Get extra leader skill
        const db::unit::ExtraLeaderSkill *extra_leader_skill =
            get_unit_extra_leader_skill(*center->default_leader_skill_id);

        if (extra_leader_skill != nullptr) {
            // Apply extra leader skill
            for (size_t k = 0; k < result.size(); k++) {
                LiveDeckStats &stat = result[k];
                const LiveDeckStats &base_stat = stats[k];
                const db::unit::Unit *unit_info = get_unit_info(player_units[k]->unit_id);

                if (has_member_tag(unit_info->unit_type_id, extra_leader_skill->member_tag_id)) {
                    auto [smile, pure, cool] = leader_skill::calculate_bonus(
                        extra_leader_skill->leader_skill_effect_type, extra_leader_skill->effect_value,
                        base_stat.smile, base_stat.cute, base_stat.cool);

                    stat.smile = stat.smile + smile;
                    stat.cute = stat.cute + pure;
                    stat.cool = stat.cool + cool;
                }
            }
        }
    }

    return result;
}

std::vector<LiveDeckUnitAttribute> TeamStatCalculator::construct_live_deck_unit_attribute(
    const std::vector<db::main::Unit *> &player_units, const std::vector<unit_model::UnitInfoData> &unit_full_data_list,
    const std::vector<LiveDeckStats> &stats) {
    std::vector<LiveDeckUnitAttribute> result;

    for (size_t i = 0; i < 9 && i < stats.size() && i < player_units.size() && i < unit_full_data_list.size(); i++) {
        const LiveDeckStats &stat = stats[i];
        const db::main::Unit *unit_data = player_units[i];
        const unit_model::UnitInfoData &unit_full_data = unit_full_data_list[i];

        LiveDeckUnitAttribute attr;
        attr.unit_id = unit_data->unit_id;
        attr.level = unit_full_data.level;
        attr.love = unit_data->love;
        attr.rank = unit_data->rank;
        attr.display_rank = unit_data->display_rank;
        attr.smile = stat.smile;
        attr.cute = stat.cute;
        attr.cool = stat.cool;
        attr.is_love_max = unit_full_data.is_love_max;
        attr.is_rank_max = unit_full_data.is_rank_max;
        attr.is_level_max = unit_full_data.is_level_max;
        attr.unit_skill_exp = unit_data->skill_exp;
        attr.unit_skill_level = unit_full_data.unit_skill_level;
        attr.unit_removable_skill_capacity = unit_data->unit_removable_skill_capacity;
        attr.removable_skill_ids = unit::get_unit_removable_skills(context, *unit_data);
        attr.position = (int64_t)i + 1;
        result.push_back(std::move(attr));
    }

    return result;
}

std::vector<LiveDeckStats> TeamStatCalculator::add_live_deck_stats_list(const std::vector<LiveDeckStats> &a,
                                                                        const std::vector<LiveDeckStats> &b) {
    std::vector<LiveDeckStats> out;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) out.push_back(a[i] + b[i]);
    return out;
}

namespace {

// functools.reduce(LiveDeckStats.__add__, list)
LiveDeckStats reduce_stats(const std::vector<LiveDeckStats> &list) {
    if (list.empty()) throw std::invalid_argument("reduce() of empty iterable with no initial value");
    LiveDeckStats total = list[0];
    for (size_t i = 1; i < list.size(); i++) total = total + list[i];
    return total;
}

}  // namespace

LiveDeckInfo TeamStatCalculator::get_live_stats(int64_t unit_deck_id, const std::vector<db::main::Unit *> &player_units,
                                                db::main::Unit &guest, const museum::MuseumParameterData &museum_param) {
    // A few references:
    // https://github.com/NonSpicyBurrito/sif-team-simulator/blob/2b018170b509f93c0bff4f8f56e6cebd07c7f7fc/src/core/stats.ts
    // https://web.archive.org/web/20181212085822/http://decaf.kouhi.me/lovelive/index.php?title=Scoring
    // Note that the rest are trial-and-error, but it's assured that this method is exactly same as in calculated
    // score by the client.

    auto [base_stat, unit_full_data_list] = get_base_team_stats(player_units);
    auto love_stat = add_live_deck_stats_list(base_stat, apply_bond_to_stat(player_units));
    auto sis_stat_base = apply_sis_stats(player_units, love_stat);
    auto sis_stat = add_live_deck_stats_list(love_stat, sis_stat_base);
    auto museum_stat = add_live_deck_stats_list(sis_stat, apply_museum_stats(museum_param));
    auto leader_stat = apply_leader_stats(player_units, museum_stat, *player_units.at(4));
    auto guest_stat = apply_leader_stats(player_units, museum_stat, guest);

    auto final_stat = add_live_deck_stats_list(add_live_deck_stats_list(museum_stat, leader_stat), guest_stat);
    LiveDeckStats total = reduce_stats(final_stat);
    LiveDeckStats leader = reduce_stats(leader_stat);

    LiveDeckInfo info;
    info.unit_deck_id = unit_deck_id;
    info.total_smile = total.smile;
    info.total_cute = total.cute;
    info.total_cool = total.cool;
    info.total_hp = total.hp;
    info.total_status = total;
    info.center_bonus = leader;
    info.si_bonus = reduce_stats(sis_stat_base);
    info.unit_list = construct_live_deck_unit_attribute(player_units, unit_full_data_list, final_stat);
    return info;
}

const db::unit::Unit *TeamStatCalculator::get_unit_info(int64_t unit_id) {
    const db::unit::Unit *unit_info = unit::get_unit_info(context, unit_id);
    if (unit_info == nullptr) throw std::invalid_argument("invalid unit_id (info is None)");
    return unit_info;
}

const db::unit::LeaderSkill *TeamStatCalculator::get_unit_leader_skill(int64_t leader_skill) {
    const db::unit::LeaderSkill *unit_leader_skill = unit::get_leader_skill(context, leader_skill);
    if (unit_leader_skill == nullptr) throw std::invalid_argument("invalid leader_skill (unit_leader_skill is None)");
    return unit_leader_skill;
}

const db::unit::ExtraLeaderSkill *TeamStatCalculator::get_unit_extra_leader_skill(int64_t leader_skill) {
    return unit::get_extra_leader_skill(context, leader_skill);
}

bool TeamStatCalculator::has_member_tag(int64_t unit_type_id, int64_t member_tag_id) {
    return unit::unit_type_has_tag(context, unit_type_id, member_tag_id);
}

std::string get_item_name(Context &context, const model::Ptr<item_model::Item> &item_data) {
    return get_item_name(context, item_data->add_type, item_data->item_id);
}

std::string get_item_name(Context &context, const_::ADD_TYPE add_type, int64_t item_id) {
    switch (add_type) {
    case const_::ADD_TYPE::NONE:
        return "None";
    case const_::ADD_TYPE::ITEM: {
        // Query kg_item_m
        const db::item::KGItem *item_info = db::get_decrypted_row<db::item::KGItem>(context.db.item, item_id);
        if (item_info != nullptr) {
            std::optional<std::string> name = context.get_text(item_info->name, item_info->name_en);
            if (name && !name->empty()) return *name;
            return "Unknown Item #" + std::to_string(item_id);
        }
        break;
    }
    case const_::ADD_TYPE::UNIT: {
        // Query unit info
        const db::unit::Unit *unit_info = unit::get_unit_info(context, item_id);
        if (unit_info != nullptr) {
            std::optional<std::string> eponym = context.get_text(unit_info->eponym, unit_info->eponym_en);
            const std::string &unit_name = context.get_text(unit_info->name, unit_info->name_en);
            if (!eponym) return "#" + std::to_string(unit_info->unit_number) + " - " + unit_name;
            return "#" + std::to_string(unit_info->unit_number) + " - \"" + *eponym + "\" " + unit_name;
        }
        break;
    }
    case const_::ADD_TYPE::EXCHANGE_POINT: {
        // Query stickers
        const db::exchange::ExchangePoint *exchange_info =
            db::get_decrypted_row<db::exchange::ExchangePoint>(context.db.exchange, item_id);
        if (exchange_info != nullptr) return context.get_text(exchange_info->name, exchange_info->name_en);
        break;
    }
    case const_::ADD_TYPE::LIVE: {
        // Query live
        const db::live::LiveTrack *live_track_info = live::get_live_track_info(context, item_id);
        if (live_track_info != nullptr) {
            const std::string &live_name = context.get_text(live_track_info->name, live_track_info->name_en);
            return "\xe2\x99\xaa " + live_name + " \xe2\x99\xaa";
        }
        break;
    }
    case const_::ADD_TYPE::AWARD: {
        // Query award_m
        const db::item::Award *award_info = db::get_decrypted_row<db::item::Award>(context.db.item, item_id);
        if (award_info != nullptr) return context.get_text(award_info->name, award_info->name_en);
        break;
    }
    case const_::ADD_TYPE::BACKGROUND: {
        // Query background_m
        const db::item::Background *bg_info = db::get_decrypted_row<db::item::Background>(context.db.item, item_id);
        if (bg_info != nullptr) return context.get_text(bg_info->name, bg_info->name_en);
        break;
    }
    case const_::ADD_TYPE::SCENARIO: {
        // Query scenario_m and scenario_chapter_m
        const db::scenario::Scenario *scenario_info =
            db::get_decrypted_row<db::scenario::Scenario>(context.db.scenario, item_id);
        if (scenario_info != nullptr) {
            const db::scenario::Chapter *scenario_chapter_info = db::get_decrypted_row<db::scenario::Chapter>(
                context.db.scenario, scenario_info->scenario_chapter_id);
            if (scenario_chapter_info != nullptr) {
                const std::string &scenario_chapter_name =
                    context.get_text(scenario_chapter_info->name, scenario_chapter_info->name_en);
                const std::string &scenario_name = context.get_text(scenario_info->title, scenario_info->title_en);
                return scenario_chapter_name + " - " + scenario_name;
            }
        }
        break;
    }
    case const_::ADD_TYPE::SCHOOL_IDOL_SKILL: {
        // Query unit_removable_skill_m
        const db::unit::RemovableSkill *unit_removable_skill_info =
            unit::get_removable_skill_game_info(context, item_id);
        if (unit_removable_skill_info != nullptr) {
            std::optional<std::string> name =
                context.get_text(unit_removable_skill_info->name, unit_removable_skill_info->name_en);
            if (name && !name->empty()) return *name;
            return "Unknown SIS #" + std::to_string(item_id);
        }
        break;
    }
    default: {
        // Query add_type_m
        const db::game_mater::AddType *add_type_info =
            context.db.game_mater.get<db::game_mater::AddType>((int64_t)add_type);
        if (add_type_info != nullptr) return context.get_text(add_type_info->name, add_type_info->name_en);
        break;
    }
    }

    return "Unknown (add_type " + std::to_string((int64_t)add_type) + ", item_id " + std::to_string(item_id) + ")";
}

model::Ptr<item_model::Item> deserialize_item_data(Context &context, const item_model::BaseItem &item_base) {
    model::Ptr<item_model::Item> item_data;
    switch (item_base.add_type) {
    case const_::ADD_TYPE::UNIT: {
        unit_model::UnitExtraData unit_extra_data = unit_model::UnitExtraData_EMPTY;

        bool has_extra = !item_base.extra_data.is_null() &&
                         !(item_base.extra_data.is_object() && item_base.extra_data.empty()) &&
                         !(item_base.extra_data.is_array() && item_base.extra_data.empty());
        if (has_extra) {
            try {
                unit_extra_data = model::parse<unit_model::UnitExtraData>(item_base.extra_data);
            } catch (const error::ValidationError &) {
            }
        }

        item_data = unit::create_unit_item(context, item_base.item_id, item_base.amount, unit_extra_data);
        break;
    }
    case const_::ADD_TYPE::SCENARIO: {
        auto scenario_item = model::make<scenario_model::ScenarioItem>();
        scenario_item->item_id = item_base.item_id;
        scenario_item->amount = item_base.amount;
        item_data = scenario_item;
        break;
    }
    case const_::ADD_TYPE::LIVE: {
        auto live_item = model::make<live_model::LiveItem>();
        live_item->item_id = item_base.item_id;
        live_item->amount = item_base.amount;
        item_data = live_item;
        break;
    }
    default: {
        auto plain = model::make<item_model::Item>();
        plain->add_type = item_base.add_type;
        plain->item_id = item_base.item_id;
        plain->amount = item_base.amount;
        item_data = plain;
        break;
    }
    }
    item_data->item_category_id = item::get_item_category(context, item_data);
    return item_data;
}

}  // namespace npps::system::advanced
