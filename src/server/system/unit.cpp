// npps4/system/unit.py
#include "system/unit.hpp"

#include "app/error.hpp"
#include "system/album.hpp"
#include "system/exchange.hpp"
#include "system/reward.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace npps::system::unit {

namespace {

// @context_cacheable for values that are cached even when "empty" (non-pointer results).
template <typename V, typename K, typename F>
const V &cached_ref(Context &context, const char *key, const K &id, F &&miss) {
    auto &cache = context.memo<std::map<K, V>>(key);
    auto it = cache.find(id);
    if (it == cache.end()) it = cache.emplace(id, miss()).first;
    return it->second;
}

// Python sequence[i] including negative indices.
template <typename V> const typename V::value_type &py_at(const V &v, int64_t i) {
    int64_t n = (int64_t)v.size();
    if (i < 0) i += n;
    if (i < 0 || i >= n) throw std::out_of_range("list index out of range");
    return v[(size_t)i];
}

int64_t py_clamp(int64_t v, int64_t lo, int64_t hi) { return std::min(std::max(v, lo), hi); }

}  // namespace

UnitStatsCalculationID UnitStatsCalculationID::from_unit_data(const db::main::Unit &unit_data) {
    UnitStatsCalculationID id;
    id.unit_id = unit_data.unit_id;
    id.exp = unit_data.exp;
    id.max_level = unit_data.max_level;
    id.level_limit_id = unit_data.level_limit_id;
    return id;
}

int64_t count_units(Context &context, const db::main::User &user, bool active) {
    return context.db.main.count<db::main::Unit>("WHERE user_id = ? AND active = ?", user.id, active);
}

std::vector<db::main::Unit *> get_all_units(Context &context, const db::main::User &user, std::optional<bool> active) {
    if (active) return context.db.main.select<db::main::Unit>("WHERE user_id = ? AND active = ?", user.id, *active);
    return context.db.main.select<db::main::Unit>("WHERE user_id = ?", user.id);
}

std::optional<db::main::Unit> create_unit(Context &context, const db::main::User *user, int64_t unit_id, bool active,
                                          int64_t level, bool is_signed) {
    const db::unit::Unit *unit_info = get_unit_info(context, unit_id);
    if (unit_info->disable_rank_up > 0) return std::nullopt;

    const db::unit::Rarity *rarity = context.db.unit.get<db::unit::Rarity>(unit_info->rarity);
    if (!rarity) throw std::runtime_error("cannot get rarity (is db corrupt?)");

    int64_t max_level = unit_info->rank_min == unit_info->rank_max ? rarity->after_level_max : rarity->before_level_max;

    db::main::Unit unit_data;
    unit_data.user_id = user ? user->id : 0;
    unit_data.unit_id = unit_id;
    unit_data.active = active;
    unit_data.favorite_flag = false;
    unit_data.is_signed = is_signed;
    unit_data.insert_date = util::time();
    unit_data.exp = 0;
    unit_data.skill_exp = 0;
    unit_data.max_level = max_level;
    unit_data.love = 0;
    unit_data.rank = unit_info->rank_min;
    unit_data.display_rank = unit_info->rank_min;
    unit_data.level_limit_id = 0;
    unit_data.unit_removable_skill_capacity = unit_info->default_removable_skill_capacity;

    const LevelUpPatterns &unit_level_up_pattern = get_unit_level_up_pattern(context, unit_info->unit_level_up_pattern_id);
    unit_data.exp = get_exp_for_target_level(*unit_info, unit_level_up_pattern, level);

    if (unit_info->rarity == 4) {
        // FIXME: Determine if it's promo card and set to 2 in that case
        unit_data.level_limit_id = 1;
    }

    return unit_data;
}

db::main::Unit *add_unit_by_object(Context &context, db::main::User &user, const db::main::Unit &unit_data) {
    const db::unit::Unit *unit_info = get_unit_info(context, unit_data.unit_id);
    const db::unit::Rarity *rarity = get_unit_rarity(context, unit_info->rarity);
    if (!rarity) throw std::invalid_argument("unit rarity not found");

    UnitStatsResult stats = get_unit_stats_from_unit_data(context, UnitStatsCalculationID::from_unit_data(unit_data));
    db::main::Unit *added = context.db.main.add(unit_data);
    album::update(context, user, unit_data.unit_id, unit_data.rank >= unit_info->rank_max,
                  unit_data.love >= rarity->after_love_max, stats.level >= rarity->after_level_max);
    context.db.main.flush();
    return added;
}

db::main::Unit *add_unit_simple(Context &context, db::main::User &user, int64_t unit_id, bool active,
                                const unit_model::UnitExtraData &extra_data) {
    auto unit_item = create_unit_item(context, unit_id, 1, extra_data);
    if (auto full = std::dynamic_pointer_cast<unit_model::UnitItem>(unit_item)) {
        db::main::Unit unit_data = create_unit_data(context, user, *full, active);
        return add_unit_by_object(context, user, unit_data);
    }
    add_supporter_unit(context, user, unit_id);
    return nullptr;
}

db::main::Unit *get_unit(Context &context, int64_t unit_owning_user_id) {
    db::main::Unit *result = context.db.main.get<db::main::Unit>(unit_owning_user_id);
    if (!result) throw error::by_code(error::ERROR_CODE_UNIT_NOT_EXIST);
    return result;
}

void validate_unit(const db::main::User &user, const db::main::Unit *unit_data) {
    if (!unit_data || unit_data->user_id != user.id) throw error::by_code(error::ERROR_CODE_UNIT_NOT_EXIST);
}

db::main::UnitSupporter *get_supporter_unit(Context &context, const db::main::User &user, int64_t unit_id,
                                            bool ensure) {
    const db::unit::Unit *unit_info = get_unit_info(context, unit_id);
    if (unit_info->disable_rank_up == 0) return nullptr;

    db::main::UnitSupporter *unitsupp =
        context.db.main.first<db::main::UnitSupporter>("WHERE user_id = ? AND unit_id = ?", user.id, unit_id);

    if (!unitsupp && ensure) {
        db::main::UnitSupporter row;
        row.user_id = user.id;
        row.unit_id = unit_id;
        row.amount = 0;
        unitsupp = context.db.main.add(row);
    }

    return unitsupp;
}

bool add_supporter_unit(Context &context, db::main::User &user, int64_t unit_id, int64_t quantity) {
    if (quantity < 1) throw std::invalid_argument("invalid amount");

    db::main::UnitSupporter *unitsupp = get_supporter_unit(context, user, unit_id, true);
    if (!unitsupp) return false;

    unitsupp->amount = unitsupp->amount + quantity;
    album::update(context, user, unit_id, true, true, true);
    context.db.main.flush();
    return true;
}

bool sub_supporter_unit(Context &context, db::main::User &user, int64_t unit_id, int64_t quantity) {
    if (quantity < 1) throw std::invalid_argument("invalid amount");

    db::main::UnitSupporter *unitsupp = get_supporter_unit(context, user, unit_id);

    if (unitsupp && unitsupp->amount >= quantity) {
        unitsupp->amount = unitsupp->amount - quantity;
        context.db.main.flush();
        return true;
    }

    return false;
}

std::vector<std::tuple<int64_t, int64_t>> get_all_supporter_unit(Context &context, const db::main::User &user) {
    auto rows = context.db.main.select<db::main::UnitSupporter>("WHERE user_id = ? ORDER BY unit_id", user.id);
    std::vector<std::tuple<int64_t, int64_t>> supporters;
    for (const auto *row : rows) supporters.emplace_back(row->unit_id, row->amount);
    return supporters;
}

std::vector<unit_model::SupporterInfoResponse> get_unit_support_list_response(Context &context,
                                                                              const db::main::User &user) {
    std::vector<unit_model::SupporterInfoResponse> out;
    for (const auto &[unit_id, amount] : get_all_supporter_unit(context, user))
        out.push_back(unit_model::SupporterInfoResponse::make(unit_id, amount));
    return out;
}

const db::unit::Unit *get_unit_info(Context &context, int64_t unit_id) {
    auto &cache = context.memo<std::map<int64_t, const db::unit::Unit *>>("unit");
    auto it = cache.find(unit_id);
    if (it != cache.end()) return it->second;
    const db::unit::Unit *unit_info = db::get_decrypted_row<db::unit::Unit>(context.db.unit, unit_id);
    if (!unit_info) throw std::invalid_argument("info on unit_id " + std::to_string(unit_id) + " does not exist");
    cache.emplace(unit_id, unit_info);
    return unit_info;
}

const db::unit::Rarity *get_unit_rarity(Context &context, int64_t rarity) {
    auto &cache = context.memo<std::map<int64_t, const db::unit::Rarity *>>("unit_rarity");
    auto it = cache.find(rarity);
    if (it != cache.end()) return it->second;
    const db::unit::Rarity *result = context.db.unit.get<db::unit::Rarity>(rarity);
    if (result) cache.emplace(rarity, result);
    return result;
}

const db::unit::Unit *get_unit_info_from_unit_number(Context &context, int64_t unit_number) {
    auto &cache = context.memo<std::map<int64_t, const db::unit::Unit *>>("unit_by_number");
    auto it = cache.find(unit_number);
    if (it != cache.end()) return it->second;

    const db::unit::Unit *unit_info = context.db.unit.first<db::unit::Unit>("WHERE unit_number = ?", unit_number);
    if (!unit_info) {
        // Try decrypting all rows
        for (const db::unit::Unit *encrypted : context.db.unit.select<db::unit::Unit>("WHERE unit_number = 0")) {
            const db::unit::Unit *decrypted = db::decrypt_row(encrypted);
            if (decrypted && decrypted->unit_number == unit_number) {
                unit_info = decrypted;
                break;
            }
        }
    }

    if (unit_info) cache.emplace(unit_number, unit_info);
    return unit_info;
}

const LevelUpPatterns &get_unit_level_up_pattern(Context &context, int64_t unit_level_up_pattern_id) {
    return cached_ref<LevelUpPatterns>(context, "unit_level_up_pattern", unit_level_up_pattern_id, [&] {
        return context.db.unit.select<db::unit::UnitLevelUpPattern>("WHERE unit_level_up_pattern_id = ?",
                                                                    unit_level_up_pattern_id);
    });
}

const LevelLimitPatterns &get_unit_level_limit_pattern(Context &context, int64_t level_limit_id) {
    return cached_ref<LevelLimitPatterns>(context, "unit_level_limit_pattern", level_limit_id, [&] {
        return context.db.unit.select<db::unit::LevelLimitPattern>("WHERE unit_level_limit_id = ?", level_limit_id);
    });
}

const db::unit::UnitSkill *get_unit_skill(Context &context, std::optional<int64_t> default_unit_skill_id) {
    if (!default_unit_skill_id || *default_unit_skill_id == 0) return nullptr;
    auto &cache = context.memo<std::map<int64_t, const db::unit::UnitSkill *>>("unit_skill");
    auto it = cache.find(*default_unit_skill_id);
    if (it != cache.end()) return it->second;
    const db::unit::UnitSkill *result = db::get_decrypted_row<db::unit::UnitSkill>(context.db.unit, *default_unit_skill_id);
    if (result) cache.emplace(*default_unit_skill_id, result);
    return result;
}

const SkillLevelUpPatterns &get_unit_skill_level_up_pattern(Context &context, int64_t unit_skill) {
    return cached_ref<SkillLevelUpPatterns>(context, "unit_skill_level_up_pattern", unit_skill, [&] {
        return context.db.unit.select<db::unit::UnitSkillLevelUpPattern>(
            "WHERE unit_skill_level_up_pattern_id = ? ORDER BY skill_level", unit_skill);
    });
}

bool detach_from_deck_2(int64_t unit_owning_user_id, db::main::UnitDeck &deck) {
    int64_t *slots[] = {&deck.unit_owning_user_id_1, &deck.unit_owning_user_id_2, &deck.unit_owning_user_id_3,
                        &deck.unit_owning_user_id_4, &deck.unit_owning_user_id_5, &deck.unit_owning_user_id_6,
                        &deck.unit_owning_user_id_7, &deck.unit_owning_user_id_8, &deck.unit_owning_user_id_9};
    for (int64_t *slot : slots) {
        if (*slot == unit_owning_user_id) {
            *slot = 0;
            return true;
        }
    }
    return false;
}

void detach_from_all_deck(Context &context, db::main::User &user, const db::main::Unit &unit_data) {
    // Remove from deck first
    for (db::main::UnitDeck *deck : context.db.main.select<db::main::UnitDeck>("WHERE user_id = ?", user.id)) {
        if (detach_from_deck_2(unit_data.id, *deck) && user.active_deck_index == deck->deck_number)
            throw error::by_code(error::ERROR_CODE_IGNORE_MAIN_DECK_UNIT);
    }
}

void remove_unit(Context &context, db::main::User &user, db::main::Unit *unit_data) {
    validate_unit(user, unit_data);

    if (user.center_unit_owning_user_id == unit_data->id) throw error::by_code(error::ERROR_CODE_UNIT_NOT_DISPOSE);

    // Remove from deck first
    detach_from_all_deck(context, user, *unit_data);

    // Remove SIS
    for (int64_t removable_skill_id : get_unit_removable_skills(context, *unit_data))
        detach_unit_removable_skill(context, *unit_data, removable_skill_id);

    // Remove from unit
    context.db.main.remove(unit_data);
    context.db.main.flush();
}

std::optional<std::tuple<db::main::UnitDeck *, std::vector<int64_t>>> load_unit_deck(Context &context,
                                                                                      const db::main::User &user,
                                                                                      int64_t index, bool ensure) {
    if (index < VALID_DECK_ID_FIRST || index > VALID_DECK_ID_LAST) throw std::invalid_argument("deck index out of range");

    db::main::UnitDeck *deck =
        context.db.main.first<db::main::UnitDeck>("WHERE user_id = ? AND deck_number = ?", user.id, index);
    std::vector<int64_t> deckunits(9, 0);

    if (!deck) {
        if (!ensure) return std::nullopt;

        db::main::UnitDeck row;
        row.user_id = user.id;
        row.deck_number = index;
        // TODO: Move to consts
        row.name = (context.lang == Language::en ? "Team " : "ユニット") + std::string(1, (char)(index + 64));
        deck = context.db.main.add(row);
        context.db.main.flush();
    } else {
        deckunits[0] = deck->unit_owning_user_id_1;
        deckunits[1] = deck->unit_owning_user_id_2;
        deckunits[2] = deck->unit_owning_user_id_3;
        deckunits[3] = deck->unit_owning_user_id_4;
        deckunits[4] = deck->unit_owning_user_id_5;
        deckunits[5] = deck->unit_owning_user_id_6;
        deckunits[6] = deck->unit_owning_user_id_7;
        deckunits[7] = deck->unit_owning_user_id_8;
        deckunits[8] = deck->unit_owning_user_id_9;
    }

    return std::make_tuple(deck, std::move(deckunits));
}

void save_unit_deck(Context &context, const db::main::User &user, db::main::UnitDeck &deck,
                    const std::vector<int64_t> &unit_owning_user_ids) {
    if (deck.user_id != user.id) throw std::invalid_argument("invalid deck");

    std::set<int64_t> unique_unit_owning_user_ids;
    for (int64_t uid : unit_owning_user_ids) {
        if (uid > 0) {
            if (unique_unit_owning_user_ids.count(uid)) throw std::invalid_argument("unit_owning_user_ids has duplicates");
            unique_unit_owning_user_ids.insert(uid);
        }
    }

    deck.unit_owning_user_id_1 = unit_owning_user_ids.at(0);
    deck.unit_owning_user_id_2 = unit_owning_user_ids.at(1);
    deck.unit_owning_user_id_3 = unit_owning_user_ids.at(2);
    deck.unit_owning_user_id_4 = unit_owning_user_ids.at(3);
    deck.unit_owning_user_id_5 = unit_owning_user_ids.at(4);
    deck.unit_owning_user_id_6 = unit_owning_user_ids.at(5);
    deck.unit_owning_user_id_7 = unit_owning_user_ids.at(6);
    deck.unit_owning_user_id_8 = unit_owning_user_ids.at(7);
    deck.unit_owning_user_id_9 = unit_owning_user_ids.at(8);
    context.db.main.flush();
}

std::vector<std::tuple<int64_t, std::string, std::array<int64_t, 9>>> get_all_deck_simple(Context &context,
                                                                                         const db::main::User &user) {
    std::vector<std::tuple<int64_t, std::string, std::array<int64_t, 9>>> out;
    for (const db::main::UnitDeck *deck : context.db.main.select<db::main::UnitDeck>("WHERE user_id = ?", user.id)) {
        out.emplace_back(deck->deck_number, deck->name,
                         std::array<int64_t, 9>{deck->unit_owning_user_id_1, deck->unit_owning_user_id_2,
                                                deck->unit_owning_user_id_3, deck->unit_owning_user_id_4,
                                                deck->unit_owning_user_id_5, deck->unit_owning_user_id_6,
                                                deck->unit_owning_user_id_7, deck->unit_owning_user_id_8,
                                                deck->unit_owning_user_id_9});
    }
    return out;
}

std::set<int64_t> find_all_valid_deck_number_ids(Context &context, const db::main::User &user) {
    std::set<int64_t> result;

    for (int64_t i = VALID_DECK_ID_FIRST; i <= VALID_DECK_ID_LAST; i++) {
        auto deck_data = load_unit_deck(context, user, i, false);
        if (deck_data) {
            const std::vector<int64_t> &ids = std::get<1>(*deck_data);
            if (std::all_of(ids.begin(), ids.end(), [](int64_t v) { return v != 0; })) result.insert(i);
        }
    }

    return result;
}

void set_unit_center(Context &context, db::main::User &user, const db::main::Unit *unit_data) {
    validate_unit(user, unit_data);
    user.center_unit_owning_user_id = unit_data->id;
    context.db.main.flush();
}

int64_t get_unit_center(Context &context, const db::main::User &user, bool center_team_instead_of_partner) {
    if (center_team_instead_of_partner) {
        auto current_deck = load_unit_deck(context, user, user.active_deck_index, true);
        int64_t center = std::get<1>(*current_deck).at(4);
        if (!(center > 0)) throw std::logic_error("assertion failed");
        return center;
    }

    return user.center_unit_owning_user_id;
}

bool idolize(Context &context, db::main::User &user, db::main::Unit &unit_data) {
    if (unit_data.user_id != user.id) throw std::invalid_argument("invalid unit_id");

    const db::unit::Unit *unit_info = get_unit_info(context, unit_data.unit_id);
    const db::unit::Rarity *rarity = get_unit_rarity(context, unit_info->rarity);
    if (!rarity) throw std::invalid_argument("unit rarity not found");

    if (unit_data.rank == unit_info->rank_max) {
        // Already idolized
        return false;
    }

    unit_data.rank = unit_info->rank_max;
    unit_data.display_rank = unit_info->rank_max;
    unit_data.max_level = rarity->after_level_max;

    album::update(context, user, unit_data.unit_id, true);
    context.db.main.flush();

    return true;
}

namespace {

constexpr int LOVE_POS_CALC_ORDER[9] = {4, 0, 1, 2, 3, 5, 6, 7, 8};
constexpr int64_t LOVE_POS_CALC_WEIGHT[9] = {5, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr int64_t MAX_LOVE_ADD_VALUE = 2147483647;

std::pair<std::vector<int64_t>, int64_t> get_added_love(int64_t love, const std::vector<int64_t> &current_love,
                                                        const std::vector<int64_t> &max_love) {
    std::vector<int64_t> after_love = current_love;

    // https://github.com/DarkEnergyProcessor/NPPS/blob/v3.1.x/modules/live/reward.php#L337-L369
    while (love > 0) {
        int64_t subtracted = 0;

        for (int k = 0; k < 9; k++) {
            int i = LOVE_POS_CALC_ORDER[k];
            int64_t weight = LOVE_POS_CALC_WEIGHT[k];
            int64_t old_love = after_love.at(i);
            int64_t new_love = std::min(old_love + std::min(weight, love), max_love.at(i));
            int64_t added_love = new_love - old_love;
            after_love[i] = new_love;
            subtracted = subtracted + added_love;
            love = love - added_love;

            if (love <= 0) break;
        }

        if (subtracted == 0) break;
    }

    return {after_love, love};
}

}  // namespace

common::BeforeAfter<std::vector<int64_t>> add_love_by_deck(Context &context, db::main::User &user, int64_t deck_index,
                                                           int64_t love) {
    auto deck_data = load_unit_deck(context, user, deck_index, false);
    if (!deck_data) throw std::invalid_argument("invalid deck");
    const std::vector<int64_t> &deck_unit_ids = std::get<1>(*deck_data);

    std::vector<db::main::Unit *> units;
    for (int64_t unit_id : deck_unit_ids) units.push_back(context.db.main.get<db::main::Unit>(unit_id));
    for (auto *u : units)
        if (!u) throw std::invalid_argument("incomplete deck");

    std::vector<const db::unit::Unit *> unit_infos;
    for (auto *u : units) unit_infos.push_back(get_unit_info(context, u->unit_id));

    std::vector<const db::unit::Rarity *> unit_rarities;
    for (auto *ui : unit_infos) unit_rarities.push_back(get_unit_rarity(context, ui->rarity));
    for (auto *ur : unit_rarities)
        if (!ur) throw std::invalid_argument("unit rarity retrieval error");

    std::vector<int64_t> max_loves;
    for (size_t i = 0; i < units.size(); i++)
        max_loves.push_back(units[i]->rank == unit_infos[i]->rank_max ? unit_rarities[i]->after_love_max
                                                                       : unit_rarities[i]->before_love_max);

    std::vector<int64_t> before_love;
    for (auto *u : units) before_love.push_back(u->love);
    auto [loves, remaining] = get_added_love(love, before_love, max_loves);

    std::map<int64_t, int64_t> total_unit_id_increment;  // for profile tracking

    for (size_t i = 0; i < units.size(); i++) {
        int64_t value = total_unit_id_increment[units[i]->unit_id] + loves[i] - before_love[i];
        total_unit_id_increment[units[i]->unit_id] = value;
    }

    if (remaining > 0) {
        auto extra = get_added_love(remaining, loves, std::vector<int64_t>(9, MAX_LOVE_ADD_VALUE)).first;

        for (size_t i = 0; i < units.size(); i++) {
            int64_t value = total_unit_id_increment[units[i]->unit_id] + extra[i] - loves[i];
            total_unit_id_increment[units[i]->unit_id] = value;
        }
    }

    for (size_t i = 0; i < units.size(); i++) {
        db::main::Unit *ud = units[i];
        ud->love = loves[i];

        album::update(context, user, ud->unit_id, false, ud->love >= unit_rarities[i]->after_love_max, false,
                      int64_t(0), album::Increment{total_unit_id_increment.at(ud->unit_id)});
    }

    context.db.main.flush();
    common::BeforeAfter<std::vector<int64_t>> result;
    result.before = before_love;
    result.after = loves;
    return result;
}

namespace {

template <typename P> UnitStatsResult calculate_unit_stats_impl(const db::unit::Unit &unit_info,
                                                                const std::vector<const P *> &pattern, int64_t exp) {
    if (pattern.empty()) throw std::out_of_range("list index out of range");
    UnitStatsResult result;
    result.level = 1;
    result.smile = unit_info.smile_max;
    result.pure = unit_info.pure_max;
    result.cool = unit_info.cool_max;
    result.hp = unit_info.hp_max;
    result.next_exp = 0;
    result.merge_exp = pattern[0]->merge_exp;
    result.merge_cost = pattern[0]->merge_cost;
    result.sale_price = pattern[0]->sale_price;

    for (const P *diff : pattern) {
        result.level = diff->unit_level;
        result.smile = unit_info.smile_max - diff->smile_diff;
        result.pure = unit_info.pure_max - diff->pure_diff;
        result.cool = unit_info.cool_max - diff->cool_diff;
        result.hp = unit_info.hp_max - diff->hp_diff;
        result.next_exp = diff->next_exp;
        result.merge_exp = diff->merge_exp;
        result.merge_cost = diff->merge_cost;
        result.sale_price = diff->sale_price;

        if (diff->next_exp > exp) break;
    }

    return result;
}

template <typename P> int64_t get_exp_for_target_level_impl(const std::vector<const P *> &patterns, int64_t level) {
    if (level == 1) return 0;

    for (const P *pattern : patterns)
        if (pattern->unit_level == level - 1) return pattern->next_exp;

    return py_at(patterns, -2)->next_exp;
}

}  // namespace

UnitStatsResult calculate_unit_stats(const db::unit::Unit &unit_info, const LevelUpPatterns &pattern, int64_t exp) {
    return calculate_unit_stats_impl(unit_info, pattern, exp);
}

UnitStatsResult calculate_unit_stats(const db::unit::Unit &unit_info, const LevelLimitPatterns &pattern, int64_t exp) {
    return calculate_unit_stats_impl(unit_info, pattern, exp);
}

int64_t get_exp_for_target_level(const db::unit::Unit &, const LevelUpPatterns &patterns, int64_t level) {
    return get_exp_for_target_level_impl(patterns, level);
}

int64_t get_exp_for_target_level(const db::unit::Unit &, const LevelLimitPatterns &patterns, int64_t level) {
    return get_exp_for_target_level_impl(patterns, level);
}

std::tuple<int64_t, int64_t> calculate_unit_skill_stats(const db::unit::UnitSkill *unit_skill,
                                                        const SkillLevelUpPatterns *pattern, int64_t exp) {
    if (!unit_skill || !pattern) return {1, 0};

    const auto *last = py_at(*pattern, -1);
    for (const auto *stat : *pattern)
        if (stat->next_exp > exp) return {stat->skill_level, stat->next_exp};

    return {last->skill_level, 0};
}

UnitStatsResult get_unit_stats_from_unit_data(Context &context, const UnitStatsCalculationID &calckey) {
    return cached_ref<UnitStatsResult>(context, "unit_stats_calculated", calckey, [&] {
        const db::unit::Unit *unit_info = get_unit_info(context, calckey.unit_id);
        const db::unit::Rarity *unit_rarity = get_unit_rarity(context, unit_info->rarity);
        if (!unit_rarity) throw std::logic_error("assertion failed");

        const LevelUpPatterns &levelup_pattern = get_unit_level_up_pattern(context, unit_info->unit_level_up_pattern_id);
        UnitStatsResult stats = calculate_unit_stats(*unit_info, levelup_pattern, calckey.exp);

        if (calckey.level_limit_id > 0 && stats.level >= unit_rarity->after_level_max &&
            calckey.max_level > unit_rarity->after_level_max) {
            // Use level_limit pattern
            const LevelLimitPatterns &limit_pattern = get_unit_level_limit_pattern(context, calckey.level_limit_id);
            stats = calculate_unit_stats(*unit_info, limit_pattern, calckey.exp);
        }

        return stats;
    });
}

std::tuple<unit_model::UnitInfoData, UnitStatsResult> get_unit_data_full_info(Context &context,
                                                                              const db::main::Unit &unit_data) {
    const db::unit::Unit *unit_info = get_unit_info(context, unit_data.unit_id);

    // Calculate unit level
    const db::unit::Rarity *unit_rarity = get_unit_rarity(context, unit_info->rarity);
    if (!unit_rarity) throw std::runtime_error("unit_rarity is none");

    UnitStatsResult stats = get_unit_stats_from_unit_data(context, UnitStatsCalculationID::from_unit_data(unit_data));

    // Calculate unit skill level
    const db::unit::UnitSkill *skill = get_unit_skill(context, unit_info->default_unit_skill_id);
    bool skill_max;
    int64_t skill_level;
    if (skill) {
        const SkillLevelUpPatterns &skill_levels = get_unit_skill_level_up_pattern(context, skill->unit_skill_level_up_pattern_id);

        auto skill_stats = calculate_unit_skill_stats(skill, &skill_levels, unit_data.skill_exp);
        skill_max = std::get<0>(skill_stats) == skill->max_level;
        skill_level = std::get<0>(skill_stats);
    } else {
        skill_max = true;
        skill_level = 1;
    }

    bool idolized = unit_data.rank == unit_info->rank_max;
    int64_t max_level = idolized ? unit_rarity->after_level_max : unit_rarity->before_level_max;
    int64_t max_love = idolized ? unit_rarity->after_love_max : unit_rarity->before_love_max;
    int64_t real_max_exp = stats.level == unit_rarity->before_level_max && !idolized ? 0 : stats.next_exp;
    bool removable_skill_max = unit_data.unit_removable_skill_capacity == unit_info->max_removable_skill_capacity;

    unit_model::UnitInfoData info;
    info.unit_owning_user_id = unit_data.id;
    info.unit_id = unit_data.unit_id;
    info.unit_rarity_id = unit_info->rarity;
    info.exp = unit_data.exp;
    info.next_exp = real_max_exp;
    info.level = stats.level;
    info.max_level = max_level;
    info.level_limit_id = unit_data.level_limit_id;
    info.rank = unit_data.rank;
    info.max_rank = unit_info->rank_max;
    info.love = unit_data.love;
    info.max_love = max_love;
    info.unit_skill_exp = unit_data.skill_exp;
    info.unit_skill_level = skill_level;
    info.max_hp = stats.hp;
    info.unit_removable_skill_capacity = unit_data.unit_removable_skill_capacity;
    info.favorite_flag = unit_data.favorite_flag;
    info.display_rank = unit_data.display_rank;
    info.is_rank_max = idolized;
    info.is_love_max = unit_data.love >= unit_rarity->after_love_max;
    info.is_level_max = stats.level >= unit_rarity->after_level_max;
    info.is_signed = unit_data.is_signed && has_signed_variant(context, unit_data.unit_id);
    info.is_skill_level_max = skill_max;
    info.is_removable_skill_capacity_max = removable_skill_max;
    info.insert_date = util::timestamp_to_datetime(unit_data.insert_date);
    return {std::move(info), stats};
}

std::tuple<int64_t, int64_t, int64_t> calculate_bonus_stat_of_removable_skill(
    const db::unit::RemovableSkill &removable_skill, const std::tuple<int64_t, int64_t, int64_t> &stats) {
    int64_t result[3] = {0, 0, 0};
    int64_t values[3] = {std::get<0>(stats), std::get<1>(stats), std::get<2>(stats)};

    if (removable_skill.effect_type >= 1 && removable_skill.effect_type < 4) {
        // We only care about smile/pure/cool for now
        int64_t i = removable_skill.effect_type - 1;
        if (removable_skill.fixed_value_flag) result[i] = (int64_t)std::ceil(removable_skill.effect_value);
        else result[i] = (int64_t)std::ceil((double)values[i] * removable_skill.effect_value / 100.0);
    }

    return {result[0], result[1], result[2]};
}

bool unit_type_has_tag(Context &context, int64_t unit_type_id, int64_t member_tag_id) {
    return cached_ref<bool>(context, "unit_type_member_tag", std::make_pair(unit_type_id, member_tag_id), [&] {
        return context.db.unit.first<db::unit::UnitTypeMemberTag>("WHERE unit_type_id = ? AND member_tag_id = ?",
                                                                  unit_type_id, member_tag_id) != nullptr;
    });
}

const db::unit::LeaderSkill *get_leader_skill(Context &context, int64_t leader_skill) {
    auto &cache = context.memo<std::map<int64_t, const db::unit::LeaderSkill *>>("unit_leader_skill");
    auto it = cache.find(leader_skill);
    if (it != cache.end()) return it->second;
    const db::unit::LeaderSkill *result = db::get_decrypted_row<db::unit::LeaderSkill>(context.db.unit, leader_skill);
    if (result) cache.emplace(leader_skill, result);
    return result;
}

const db::unit::ExtraLeaderSkill *get_extra_leader_skill(Context &context, int64_t leader_skill) {
    auto &cache = context.memo<std::map<int64_t, const db::unit::ExtraLeaderSkill *>>("unit_extra_leader_skill");
    auto it = cache.find(leader_skill);
    if (it != cache.end()) return it->second;
    const db::unit::ExtraLeaderSkill *result = context.db.unit.get<db::unit::ExtraLeaderSkill>(leader_skill);
    if (result) cache.emplace(leader_skill, result);
    return result;
}

db::main::RemovableSkillInfo *get_removable_skill_info(Context &context, const db::main::User &user,
                                                       int64_t removable_skill_id) {
    return context.db.main.first<db::main::RemovableSkillInfo>("WHERE user_id = ? AND unit_removable_skill_id = ?",
                                                               user.id, removable_skill_id);
}

const db::unit::RemovableSkill *get_removable_skill_game_info(Context &context, int64_t removable_skill_id) {
    auto &cache = context.memo<std::map<int64_t, const db::unit::RemovableSkill *>>("unit_removable_skill");
    auto it = cache.find(removable_skill_id);
    if (it != cache.end()) return it->second;
    const db::unit::RemovableSkill *result =
        db::get_decrypted_row<db::unit::RemovableSkill>(context.db.unit, removable_skill_id);
    if (result) cache.emplace(removable_skill_id, result);
    return result;
}

int64_t get_unit_removable_skill_count(Context &context, const db::main::User &user, int64_t removable_skill_id) {
    db::main::RemovableSkillInfo *removable_skill = get_removable_skill_info(context, user, removable_skill_id);
    return removable_skill ? removable_skill->amount : 0;
}

std::vector<db::main::RemovableSkillInfo *> get_all_unit_removable_skill(Context &context, const db::main::User &user) {
    return context.db.main.select<db::main::RemovableSkillInfo>("WHERE user_id = ?", user.id);
}

std::vector<int64_t> get_unit_removable_skills(Context &context, const db::main::Unit &unit_data) {
    std::vector<int64_t> out;
    for (const auto *sis : context.db.main.select<db::main::UnitRemovableSkill>("WHERE unit_owning_user_id = ?", unit_data.id))
        out.push_back(sis->unit_removable_skill_id);
    return out;
}

std::vector<std::pair<int64_t, std::vector<int64_t>>> get_all_unit_removable_skills(Context &context,
                                                                                    const db::main::User &user) {
    std::vector<std::pair<int64_t, std::vector<int64_t>>> sis_by_unit_id;
    std::map<int64_t, size_t> index;

    for (const auto *sis : context.db.main.select<db::main::UnitRemovableSkill>("WHERE user_id = ?", user.id)) {
        auto it = index.find(sis->unit_owning_user_id);
        if (it == index.end()) {
            it = index.emplace(sis->unit_owning_user_id, sis_by_unit_id.size()).first;
            sis_by_unit_id.emplace_back(sis->unit_owning_user_id, std::vector<int64_t>());
        }
        sis_by_unit_id[it->second].second.push_back(sis->unit_removable_skill_id);
    }

    return sis_by_unit_id;
}

int64_t add_unit_removable_skill(Context &context, db::main::User &user, int64_t removable_skill_id, int64_t amount) {
    db::main::RemovableSkillInfo *removable_skill = get_removable_skill_info(context, user, removable_skill_id);
    if (!removable_skill) {
        db::main::RemovableSkillInfo row;
        row.user_id = user.id;
        row.unit_removable_skill_id = removable_skill_id;
        row.amount = 0;
        row.insert_date = util::time();
        removable_skill = context.db.main.add(row);
    }

    removable_skill->amount = removable_skill->amount + amount;
    context.db.main.flush();
    return removable_skill->amount;
}

int64_t sub_unit_removable_skill(Context &context, db::main::User &user, int64_t removable_skill_id, int64_t amount) {
    db::main::RemovableSkillInfo *removable_skill = get_removable_skill_info(context, user, removable_skill_id);
    if (!removable_skill) return -amount;

    removable_skill->amount = removable_skill->amount - amount;
    if (removable_skill->amount >= 0) context.db.main.flush();
    return removable_skill->amount;
}

bool attach_unit_removable_skill(Context &context, const db::main::Unit &unit, int64_t removable_skill_id) {
    auto *existing = context.db.main.first<db::main::UnitRemovableSkill>(
        "WHERE unit_owning_user_id = ? AND unit_removable_skill_id = ?", unit.id, removable_skill_id);
    if (!existing) {
        db::main::UnitRemovableSkill sis;
        sis.unit_owning_user_id = unit.id;
        sis.user_id = unit.user_id;
        sis.unit_removable_skill_id = removable_skill_id;
        context.db.main.add(sis);
        context.db.main.flush();
        return true;
    }

    return false;
}

bool detach_unit_removable_skill(Context &context, const db::main::Unit &unit, int64_t removable_skill_id) {
    // sqlalchemy.delete(...) also expunges the loaded matching objects: select them and remove each.
    auto rows = context.db.main.select<db::main::UnitRemovableSkill>(
        "WHERE unit_owning_user_id = ? AND unit_removable_skill_id = ?", unit.id, removable_skill_id);
    for (auto *row : rows) context.db.main.remove(row);
    context.db.main.flush();
    return !rows.empty();
}

unit_model::RemovableSkillInfoResponse get_removable_skill_info_request(Context &context, const db::main::User &user) {
    auto owning_info = get_all_unit_removable_skill(context, user);
    auto sis_info = get_all_unit_removable_skills(context, user);

    std::map<int64_t, int64_t> used_sis;
    for (const auto &entry : sis_info)
        for (int64_t sis : entry.second) used_sis[sis] = used_sis[sis] + 1;

    unit_model::RemovableSkillInfoResponse response;
    for (const auto *i : owning_info) {
        auto it = used_sis.find(i->unit_removable_skill_id);
        response.owning_info.push_back(unit_model::OwningRemovableSkillInfo::make(
            i->unit_removable_skill_id, i->amount, it == used_sis.end() ? 0 : it->second,
            util::timestamp_to_datetime(i->insert_date)));
    }

    response.equipment_info = Json::object();
    for (const auto &[unit_owning_user_id, skills] : sis_info) {
        unit_model::EquipRemovableSkillInfo info;
        info.unit_owning_user_id = unit_owning_user_id;
        for (int64_t sis : skills)
            info.detail.push_back(model::make<unit_model::EquipRemovableSkillInfoDetail>(
                unit_model::EquipRemovableSkillInfoDetail::make(sis)));
        response.equipment_info[std::to_string(unit_owning_user_id)] = info.to_json();
    }
    return response;
}

model::Ptr<unit_model::UnitItem> unit_to_item(Context &context, const db::main::Unit &unit_data) {
    auto info = std::get<0>(get_unit_data_full_info(context, unit_data));
    auto item = model::make<unit_model::UnitItem>();
    item->unit_owning_user_id = info.unit_owning_user_id;
    item->unit_rarity_id = info.unit_rarity_id;
    item->exp = info.exp;
    item->next_exp = info.next_exp;
    item->level = info.level;
    item->level_limit_id = info.level_limit_id;
    item->max_level = info.max_level;
    item->rank = info.rank;
    item->max_rank = info.max_rank;
    item->love = info.love;
    item->max_love = info.max_love;
    item->unit_skill_level = info.unit_skill_level;
    item->max_hp = info.max_hp;
    item->favorite_flag = info.favorite_flag;
    item->display_rank = info.display_rank;
    item->unit_skill_exp = info.unit_skill_exp;
    item->unit_removable_skill_capacity = info.unit_removable_skill_capacity;
    item->is_love_max = info.is_love_max;
    item->is_level_max = info.is_level_max;
    item->is_rank_max = info.is_rank_max;
    item->is_signed = info.is_signed;
    item->is_skill_level_max = info.is_skill_level_max;
    item->is_removable_skill_capacity_max = info.is_removable_skill_capacity_max;
    item->insert_date = info.insert_date;
    item->add_type = const_::ADD_TYPE::UNIT;
    item->item_id = unit_data.unit_id;
    item->amount = 1;
    return item;
}

bool is_support_member(Context &context, int64_t unit_id) {
    return cached_ref<bool>(context, "unit_support_member", unit_id, [&] {
        const db::unit::Unit *unit_info = context.db.unit.get<db::unit::Unit>(unit_id);
        if (!unit_info) throw std::invalid_argument("invalid unit_id");
        return unit_info->disable_rank_up > 0;
    });
}

void QuickAddResult::update_unit_owning_user_id(const db::main::Unit &added) {
    if (unit_data) {
        if (full_info) full_info->unit_owning_user_id = added.id;
        if (auto item = std::dynamic_pointer_cast<unit_model::UnitItem>(as_item_reward))
            item->unit_owning_user_id = added.id;
    }
}

QuickAddResult quick_create_by_unit_add(Context &context, db::main::User &user, int64_t unit_id, int64_t level) {
    bool new_unit_flag = !album::has_ever_got_unit(context, user, unit_id);
    QuickAddResult result;
    result.unit_id = unit_id;
    if (is_support_member(context, unit_id)) {
        const db::unit::Unit *unit_info = get_unit_info(context, unit_id);
        auto item = model::make<unit_model::UnitSupportItem>();
        item->item_id = unit_id;
        item->is_support_member = true;
        item->new_unit_flag = new_unit_flag;
        item->unit_rarity_id = unit_info->rarity;
        item->attribute = unit_info->attribute_id;
        result.as_item_reward = item;
        return result;
    }

    std::optional<db::main::Unit> unit_data = create_unit(context, &user, unit_id, true, level);
    if (!unit_data) throw std::logic_error("assertion failed");
    const db::unit::Unit *unit_info = get_unit_info(context, unit_id);
    auto full = get_unit_data_full_info(context, *unit_data);
    const unit_model::UnitInfoData &info = std::get<0>(full);

    auto item = model::make<unit_model::UnitItem>();
    item->item_id = unit_id;
    item->new_unit_flag = new_unit_flag;
    item->attribute = unit_info->attribute_id;
    item->unit_owning_user_id = info.unit_owning_user_id;
    item->unit_rarity_id = info.unit_rarity_id;
    item->exp = info.exp;
    item->next_exp = info.next_exp;
    item->level = info.level;
    item->level_limit_id = info.level_limit_id;
    item->max_level = info.max_level;
    item->rank = info.rank;
    item->max_rank = info.max_rank;
    item->love = info.love;
    item->max_love = info.max_love;
    item->unit_skill_level = info.unit_skill_level;
    item->max_hp = info.max_hp;
    item->favorite_flag = info.favorite_flag;
    item->display_rank = info.display_rank;
    item->unit_skill_exp = info.unit_skill_exp;
    item->unit_removable_skill_capacity = info.unit_removable_skill_capacity;
    item->is_love_max = info.is_love_max;
    item->is_level_max = info.is_level_max;
    item->is_rank_max = info.is_rank_max;
    item->is_signed = info.is_signed;
    item->is_skill_level_max = info.is_skill_level_max;
    item->is_removable_skill_capacity_max = info.is_removable_skill_capacity_max;
    item->insert_date = info.insert_date;
    // **util.shallow_dump(unit_full_info[0]) also passes these as extras.
    item->extra["unit_id"] = info.unit_id;
    item->extra["skill_level"] = info.unit_skill_level;
    result.as_item_reward = item;
    result.unit_data = std::move(unit_data);
    result.full_info = info;
    result.stats = std::get<1>(full);
    return result;
}

int64_t process_quick_add(Context &context, db::main::User &user, QuickAddResult &quick_add_result,
                          std::optional<int64_t> current_unit_count, const std::string &reason_jp,
                          const std::string &reason_en, int64_t expire) {
    if (!current_unit_count) return count_units(context, user, true);

    int64_t count = *current_unit_count;
    // Add directly
    if (quick_add_result.unit_data) {
        if (count >= user.unit_max) {
            // Move to present box
            quick_add_result.as_item_reward->reward_box_flag = true;
            reward::add_item(context, user, quick_add_result.as_item_reward, reason_jp, reason_en, expire);
        } else {
            if (!quick_add_result.full_info) throw std::logic_error("assertion failed");
            db::main::Unit *added = add_unit_by_object(context, user, *quick_add_result.unit_data);
            // Update unit_owning_user_id
            quick_add_result.update_unit_owning_user_id(*added);
            count = count + 1;
        }
    } else {
        // Add directly
        add_supporter_unit(context, user, quick_add_result.unit_id);
    }
    return count;
}

bool has_signed_variant(Context &context, int64_t unit_id) {
    return cached_ref<bool>(context, "has_signed_variant", unit_id,
                            [&] { return context.db.unit.get<db::unit::SignAsset>(unit_id) != nullptr; });
}

bool is_unit_max(Context &context, const db::main::User &user) {
    return count_units(context, user, true) >= user.unit_max;
}

int64_t get_exchange_point_id_by_unit_id(Context &context, int64_t unit_id) {
    // TODO: Probably allow this to be configurable?
    if (exchange::is_festival_unit(context, unit_id)) return 6;

    const db::unit::Unit *unit_info = get_unit_info(context, unit_id);
    switch (unit_info->rarity) {
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    case 5: return 5;
    default: return 0;
    }
}

double get_exp_multiplier(const_::EVOLUTION_BONUS_TYPE evolution_type) {
    switch (evolution_type) {
    case const_::EVOLUTION_BONUS_TYPE::SUPER_SUCCESS: return 1.5;
    case const_::EVOLUTION_BONUS_TYPE::ULTRA_SUCCESS: return 2.0;
    default: return 1.0;
    }
}

int64_t get_max_exp(Context &context, const db::main::Unit &unit_data, const db::unit::Unit *unit_info,
                    const db::unit::Rarity *unit_rarity) {
    if (!unit_info) unit_info = get_unit_info(context, unit_data.unit_id);
    if (!unit_rarity) {
        unit_rarity = get_unit_rarity(context, unit_info->rarity);
        if (!unit_rarity) throw std::logic_error("assertion failed");
    }

    bool idolized = unit_data.rank >= unit_info->rank_max;
    bool use_user_rank = unit_data.max_level > unit_rarity->after_love_max && unit_data.level_limit_id > 0;
    int64_t normal_target_level = idolized ? unit_rarity->after_level_max : unit_rarity->before_level_max;
    int64_t target_level = use_user_rank ? std::min<int64_t>(unit_data.max_level, 500) : normal_target_level;

    if (use_user_rank) {
        const LevelLimitPatterns &levelup_pattern = get_unit_level_limit_pattern(context, unit_data.level_limit_id);
        return py_at(levelup_pattern, target_level - 100)->next_exp;
    }
    const LevelUpPatterns &levelup_pattern = get_unit_level_up_pattern(context, unit_info->unit_level_up_pattern_id);
    return py_at(levelup_pattern, target_level - 2)->next_exp;
}

const db::unit::UnitSkillLevel *get_unit_skill_level_data(Context &context, int64_t unit_skill_id, int64_t level) {
    return db::decrypt_row(context.db.unit.first<db::unit::UnitSkillLevel>("WHERE unit_skill_id = ? AND skill_level = ?",
                                                                           unit_skill_id, level));
}

int64_t get_max_skill_exp(Context &context, const db::unit::Unit &unit_info) {
    const db::unit::UnitSkill *skill = get_unit_skill(context, unit_info.default_unit_skill_id);
    if (skill) {
        const SkillLevelUpPatterns &skill_levels = get_unit_skill_level_up_pattern(context, skill->unit_skill_level_up_pattern_id);
        if (skill_levels.size() > 1) return py_at(skill_levels, -2)->next_exp;
    }

    return 0;
}

model::Ptr<unit_model::UnitSupportItem> create_unit_item(Context &context, int64_t unit_id, int64_t amount,
                                                         const unit_model::UnitExtraData &extra_data) {
    const db::unit::Unit *unit_info = get_unit_info(context, unit_id);

    if (unit_info->disable_rank_up > 0) {
        auto support = model::make<unit_model::UnitSupportItem>();
        support->item_id = unit_id;
        support->unit_rarity_id = unit_info->rarity;
        support->attribute = unit_info->attribute_id;
        support->amount = amount;
        return support;
    }

    // Calculate unit level
    const db::unit::Rarity *rarity_data = get_unit_rarity(context, unit_info->rarity);
    if (!rarity_data) throw std::runtime_error("unit_rarity is none");

    // FIXME: Determine if it's promo card and set to 2 in that case
    int64_t level_limit_id = unit_info->rarity == 4 ? 1 : 0;
    int64_t rank = py_clamp(extra_data.rank, unit_info->rank_min, unit_info->rank_max);
    bool idolized = rank == unit_info->rank_max;
    int64_t max_level = idolized ? rarity_data->after_level_max : rarity_data->before_level_max;
    int64_t max_love = idolized ? rarity_data->after_love_max : rarity_data->before_love_max;
    int64_t love = py_clamp(extra_data.love, 0, max_love);
    int64_t removable_skill_capacity =
        py_clamp(extra_data.unit_removable_skill_capacity.value_or(unit_info->default_removable_skill_capacity), 0,
                 unit_info->max_removable_skill_capacity);
    bool removable_skill_max = removable_skill_capacity == unit_info->max_removable_skill_capacity;

    int64_t exp;
    if (extra_data.level) {
        const LevelUpPatterns &unit_level_up_pattern = get_unit_level_up_pattern(context, unit_info->unit_level_up_pattern_id);
        exp = get_exp_for_target_level(*unit_info, unit_level_up_pattern, *extra_data.level);
    } else {
        exp = extra_data.exp;
    }

    // Get next EXP
    UnitStatsCalculationID calckey;
    calckey.unit_id = unit_id;
    calckey.exp = exp;
    calckey.max_level = max_level;
    calckey.level_limit_id = level_limit_id;
    UnitStatsResult stats = get_unit_stats_from_unit_data(context, calckey);
    int64_t real_max_exp = stats.level == rarity_data->before_level_max && !idolized ? 0 : stats.next_exp;

    // Calculate unit skill level
    const db::unit::UnitSkill *skill = get_unit_skill(context, unit_info->default_unit_skill_id);
    bool skill_max;
    int64_t skill_level;
    if (skill) {
        const SkillLevelUpPatterns &skill_levels = get_unit_skill_level_up_pattern(context, skill->unit_skill_level_up_pattern_id);

        auto skill_stats = calculate_unit_skill_stats(skill, &skill_levels, extra_data.skill_exp);
        skill_max = std::get<0>(skill_stats) == skill->max_level;
        skill_level = std::get<0>(skill_stats);
    } else {
        skill_max = true;
        skill_level = 1;
    }

    auto item = model::make<unit_model::UnitItem>();
    item->item_id = unit_id;
    item->amount = amount;
    item->unit_owning_user_id = 0;
    item->unit_rarity_id = unit_info->rarity;
    item->exp = exp;
    item->next_exp = real_max_exp;
    item->level = stats.level;
    item->max_level = max_level;
    item->level_limit_id = level_limit_id;
    item->rank = rank;
    item->max_rank = unit_info->rank_max;
    item->love = love;
    item->max_love = max_love;
    item->unit_skill_exp = extra_data.skill_exp;
    item->unit_skill_level = skill_level;
    item->max_hp = stats.hp;
    item->unit_removable_skill_capacity = removable_skill_capacity;
    item->favorite_flag = false;
    item->display_rank = std::max(extra_data.display_rank, unit_info->rank_min);
    item->is_rank_max = idolized;
    item->is_love_max = love >= rarity_data->after_love_max;
    item->is_level_max = stats.level >= rarity_data->after_level_max;
    item->is_signed = extra_data.is_signed;
    item->is_skill_level_max = skill_max;
    item->is_removable_skill_capacity_max = removable_skill_max;
    item->insert_date = util::timestamp_to_datetime(util::time());
    item->attribute = unit_info->attribute_id;
    return item;
}

db::main::Unit create_unit_data(Context &, const db::main::User &user, const unit_model::UnitItem &unit_item,
                                bool active) {
    db::main::Unit unit_data;
    unit_data.user_id = user.id;
    unit_data.unit_id = unit_model::unit_id(unit_item);
    unit_data.active = active;
    unit_data.favorite_flag = unit_item.favorite_flag;
    unit_data.is_signed = unit_item.is_signed;
    unit_data.insert_date = util::time();
    unit_data.exp = unit_item.exp;
    unit_data.skill_exp = unit_item.unit_skill_exp;
    unit_data.max_level = unit_item.max_level;
    unit_data.love = unit_item.love;
    unit_data.rank = unit_item.rank;
    unit_data.display_rank = unit_item.display_rank;
    unit_data.level_limit_id = unit_item.level_limit_id;
    unit_data.unit_removable_skill_capacity = unit_item.unit_removable_skill_capacity;
    return unit_data;
}

model::Ptr<unit_model::UnitItem> unit_info_data_to_unit_item(Context &context,
                                                             const unit_model::UnitInfoData &unit_info_data) {
    const db::unit::Unit *unit_info = get_unit_info(context, unit_info_data.unit_id);

    auto item = model::make<unit_model::UnitItem>();
    item->item_id = unit_info_data.unit_id;
    item->unit_owning_user_id = unit_info_data.unit_owning_user_id;
    item->unit_rarity_id = unit_info_data.unit_rarity_id ? *unit_info_data.unit_rarity_id : unit_info->rarity;
    item->exp = unit_info_data.exp;
    item->next_exp = unit_info_data.next_exp;
    item->level = unit_info_data.level;
    item->max_level = unit_info_data.max_level;
    item->level_limit_id = unit_info_data.level_limit_id;
    item->rank = unit_info_data.rank;
    item->max_rank = unit_info_data.max_rank;
    item->love = unit_info_data.love;
    item->max_love = unit_info_data.max_love;
    item->unit_skill_exp = unit_info_data.unit_skill_exp;
    item->unit_skill_level = unit_model::skill_level(unit_info_data);
    item->max_hp = unit_info_data.max_hp;
    item->unit_removable_skill_capacity = unit_info_data.unit_removable_skill_capacity;
    item->favorite_flag = unit_info_data.favorite_flag;
    item->display_rank = unit_info_data.display_rank;
    item->is_rank_max = unit_info_data.is_rank_max;
    item->is_love_max = unit_info_data.is_love_max;
    item->is_level_max = unit_info_data.is_level_max;
    item->is_signed = unit_info_data.is_signed;
    item->is_skill_level_max = unit_info_data.is_skill_level_max;
    item->is_removable_skill_capacity_max = unit_info_data.is_removable_skill_capacity_max;
    item->insert_date = unit_info_data.insert_date;
    item->attribute = unit_info->attribute_id;
    return item;
}

void populate_unit_item_to_other(const model::Ptr<unit_model::UnitSupportItem> &unit_item,
                                 game::reward::IncentiveItem &other) {
    if (!unit_item) throw std::invalid_argument("expected UnitItem or UnitSupportItem");
    // (UnitItem | UnitSupportItem) fields and computed fields except the Item fields. The Python set iteration
    // order is arbitrary; the dump order is used here.
    static const char *const ITEM_FIELDS[] = {"add_type", "item_id", "amount", "item_category_id", "reward_box_flag", "comment"};
    Json dumped = unit_item->to_json();
    for (auto &member : dumped.members()) {
        bool is_item_field = false;
        for (const char *name : ITEM_FIELDS)
            if (member.first == name) is_item_field = true;
        if (is_item_field) continue;
        if (member.first == "insert_date") other.insert_date = member.second.as_string();
        else other.extra[member.first] = member.second;
    }
}

std::tuple<std::array<int64_t, 9>, std::array<int64_t, 9>> tupleize_unit_id_and_type(
    Context &context, const std::vector<int64_t> &unit_ids) {
    std::vector<int64_t> unit_type_ids;
    for (int64_t u : unit_ids) unit_type_ids.push_back(get_unit_info(context, u)->unit_type_id);
    std::array<int64_t, 9> ids{}, types{};
    for (size_t i = 0; i < 9; i++) {
        ids[i] = unit_ids.at(i);
        types[i] = unit_type_ids.at(i);
    }
    return {ids, types};
}

}  // namespace npps::system::unit
