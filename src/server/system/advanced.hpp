#pragma once
// npps4/system/advanced.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace npps::system::advanced {

// TODO: Replace with simple boolean.
struct AddResult {
    bool success = false;
    explicit operator bool() const { return success; }
};

// LiveDeckStats.__add__
inline LiveDeckStats operator+(const LiveDeckStats &a, const LiveDeckStats &b) {
    LiveDeckStats r;
    r.smile = a.smile + b.smile;
    r.cute = a.cute + b.cute;
    r.cool = a.cool + b.cool;
    r.hp = a.hp + b.hp;
    return r;
}

// `i` is common.AnyItem. It is mutated in place (UnitItem.unit_owning_user_id, LiveItem status lists).
AddResult add_item(Context &context, db::main::User &user, const model::Ptr<item_model::Item> &i);

PartyInfo get_user_guest_party_info(Context &context, db::main::User &user);

std::vector<db::main::User *> get_random_user_for_partylist(Context &context, db::main::User &user,
                                                            bool include_current = true, int64_t limit = 3);

// Throws error::IdolError on invalid names.
void test_name(Context &context, const std::string &name);

// Certain reward can only be given once. This function replaces them to give 1 loveca instead.
// Items that are not replaced are the same objects as in `rewardss`.
std::vector<std::vector<model::Ptr<item_model::Item>>> fixup_achievement_reward(
    Context &context, db::main::User &user, const std::vector<std::vector<model::Ptr<item_model::Item>>> &rewardss);

class TeamStatCalculator {
public:
    explicit TeamStatCalculator(Context &context) : context(context) {}

    Context &context;

    std::tuple<std::vector<LiveDeckStats>, std::vector<unit_model::UnitInfoData>> get_base_team_stats(
        const std::vector<db::main::Unit *> &player_units);
    std::vector<LiveDeckStats> apply_bond_to_stat(const std::vector<db::main::Unit *> &player_units);
    static std::vector<LiveDeckStats> apply_museum_stats(const museum::MuseumParameterData &museum_param);
    std::vector<LiveDeckStats> apply_sis_stats(const std::vector<db::main::Unit *> &player_units,
                                               const std::vector<LiveDeckStats> &stats);
    std::vector<LiveDeckStats> apply_leader_stats(const std::vector<db::main::Unit *> &player_units,
                                                  const std::vector<LiveDeckStats> &stats,
                                                  db::main::Unit &leader_unit);
    std::vector<LiveDeckUnitAttribute> construct_live_deck_unit_attribute(
        const std::vector<db::main::Unit *> &player_units,
        const std::vector<unit_model::UnitInfoData> &unit_full_data_list, const std::vector<LiveDeckStats> &stats);
    static std::vector<LiveDeckStats> add_live_deck_stats_list(const std::vector<LiveDeckStats> &a,
                                                               const std::vector<LiveDeckStats> &b);
    LiveDeckInfo get_live_stats(int64_t unit_deck_id, const std::vector<db::main::Unit *> &player_units,
                                db::main::Unit &guest, const museum::MuseumParameterData &museum_param);

    // These throw std::invalid_argument (ValueError) instead of returning null, except the extra leader skill.
    const db::unit::Unit *get_unit_info(int64_t unit_id);
    const db::unit::LeaderSkill *get_unit_leader_skill(int64_t leader_skill);
    const db::unit::ExtraLeaderSkill *get_unit_extra_leader_skill(int64_t leader_skill);
    bool has_member_tag(int64_t unit_type_id, int64_t member_tag_id);
};

std::string get_item_name(Context &context, const model::Ptr<item_model::Item> &item_data);
std::string get_item_name(Context &context, const_::ADD_TYPE add_type, int64_t item_id);

// Returns common.AnyItem. Only the BaseItem fields of `item_base` are read, so any subclass may be passed.
model::Ptr<item_model::Item> deserialize_item_data(Context &context, const item_model::BaseItem &item_base);
inline model::Ptr<item_model::Item> deserialize_item_data(Context &context,
                                                          const model::Ptr<item_model::BaseItem> &item_base) {
    return deserialize_item_data(context, *item_base);
}

}  // namespace npps::system::advanced
