// npps4/system/achievement.py
#include "system/achievement.hpp"

#include "data/data.hpp"
#include "system/advanced.hpp"
#include "system/album.hpp"
#include "system/common.hpp"
#include "system/item.hpp"
#include "system/reward.hpp"
#include "system/unit.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>

namespace npps::system::achievement {

using Ach = db::achievement::Achievement;

const std::vector<item_model::BaseItem> ACHIEVEMENT_REWARD_DEFAULT = {[] {
    item_model::BaseItem b;  // item.base_loveca(1)
    b.add_type = const_::ADD_TYPE::LOVECA;
    b.item_id = 4;
    b.amount = 1;
    return b;
}()};

AchievementData from_sqlalchemy(const db::main::Achievement &ach, const Ach &info,
                                const std::vector<model::Ptr<item_model::Item>> &rewards) {
    AchievementData d;
    d.achievement_id = ach.achievement_id;
    d.count = ach.count;
    d.is_accomplished = ach.is_accomplished;
    d.insert_date = util::timestamp_to_datetime(ach.insert_date);
    if (ach.end_date == 0) d.end_date = std::nullopt;
    else d.end_date = util::timestamp_to_datetime(ach.end_date);
    d.remaining_time = "";  // FIXME: What to put here?
    d.is_new = ach.is_new;
    d.for_display = info.display_flag != 0;
    d.is_locked = false;
    d.accomplish_id = ach.is_accomplished && !ach.is_reward_claimed ? std::to_string(ach.id) : "";
    for (const auto &r : rewards) d.reward_list.push_back(model::dump(r));
    return d;
}

template <typename T> static bool contains(const std::vector<T> &v, const T &x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

void AchievementContext::extend(const AchievementContext &other) {
    accomplished.insert(accomplished.end(), other.accomplished.begin(), other.accomplished.end());
    new_.insert(new_.end(), other.new_.begin(), other.new_.end());
}

AchievementContext &AchievementContext::fix() {
    // Anything in "new" should be removed if it's in "accomplished"
    std::set<int64_t> accomplished_ids;
    for (const auto *ach : accomplished) accomplished_ids.insert(ach->achievement_id);
    std::vector<db::main::Achievement *> kept;
    for (auto *ach : new_)
        if (!accomplished_ids.count(ach->achievement_id)) kept.push_back(ach);
    new_ = std::move(kept);
    return *this;
}

void AchievementContext::add(db::main::Achievement *ach) {
    if (ach->is_accomplished) {
        if (!contains(accomplished, ach)) accomplished.push_back(ach);
        auto it = std::find(new_.begin(), new_.end(), ach);
        if (it != new_.end()) new_.erase(it);
    } else {
        auto it = std::find(accomplished.begin(), accomplished.end(), ach);
        if (it != accomplished.end()) accomplished.erase(it);
        if (!contains(new_, ach)) new_.push_back(ach);
    }
}

AchievementContext AchievementContext::operator+(const AchievementContext &other) const {
    AchievementContext r;
    r.accomplished = accomplished;
    r.accomplished.insert(r.accomplished.end(), other.accomplished.begin(), other.accomplished.end());
    r.new_ = new_;
    r.new_.insert(r.new_.end(), other.new_.begin(), other.new_.end());
    r.fix();
    return r;
}

bool AchievementContext::has_achievement() const { return !accomplished.empty() && !new_.empty(); }

// ---------------------------------------------------------------- queries on master data

static std::string placeholders(size_t n) {
    std::string s = "(";
    for (size_t i = 0; i < n; i++) s += i ? ", ?" : "?";
    return s + ")";
}

const Ach *get_achievement_info(Context &context, int64_t achievement_id) {
    const Ach *info = db::get_decrypted_row<Ach>(context.db.achievement, achievement_id);
    if (info == nullptr) throw std::invalid_argument("invalid achievement");
    return info;
}

std::vector<int64_t> get_next_achievement_ids(Context &context, int64_t achievement_id) {
    return common::get_cached<std::vector<int64_t>>(
        context, "achievement_story", achievement_id, [](Context &c, int64_t id) {
            std::vector<int64_t> out;
            for (const auto *s : c.db.achievement.select<db::achievement::Story>("WHERE achievement_id = ?", id))
                out.push_back(s->next_achievement_id);
            return out;
        });
}

std::vector<int64_t> get_prerequisite_achievement_ids(Context &context, int64_t achievement_id) {
    return common::get_cached<std::vector<int64_t>>(
        context, "achievement_reverse_story", achievement_id, [](Context &c, int64_t id) {
            std::vector<int64_t> out;
            for (const auto *s : c.db.achievement.select<db::achievement::Story>("WHERE next_achievement_id = ?", id))
                out.push_back(s->achievement_id);
            return out;
        });
}

std::vector<std::tuple<int64_t, int64_t>> get_unit_type_groups(Context &context,
                                                                int64_t achievement_unit_type_group_id) {
    return common::get_cached<std::vector<std::tuple<int64_t, int64_t>>>(
        context, "achievement_unit_type_group", achievement_unit_type_group_id, [](Context &c, int64_t id) {
            std::vector<std::tuple<int64_t, int64_t>> out;
            for (const auto *o : c.db.achievement.select<db::achievement::UnitTypeGroup>(
                     "WHERE achievement_unit_type_group_id = ?", id))
                out.emplace_back(o->unit_type_id, o->rank);
            return out;
        });
}

std::vector<int64_t> get_achievement_ids_from_category(Context &context, int64_t achievement_category_id) {
    return common::get_cached<std::vector<int64_t>>(
        context, "achievement_category", achievement_category_id, [](Context &c, int64_t id) {
            std::vector<int64_t> out;
            for (const auto *t : c.db.achievement.select<db::achievement::Tag>("WHERE achievement_category_id = ?", id))
                out.push_back(t->achievement_id);
            return out;
        });
}

// ---------------------------------------------------------------- user achievements

db::main::Achievement *add_achievement(Context &context, db::main::User &user, const Ach &ach,
                                       std::optional<int64_t> time, bool flush) {
    if (!time) time = util::time();

    db::main::Achievement user_ach;
    user_ach.achievement_id = ach.achievement_id;
    user_ach.user_id = user.id;
    user_ach.achievement_type = ach.achievement_type;
    user_ach.achievement_filter_category_id = ach.achievement_filter_category_id;
    user_ach.count = 0;
    user_ach.is_accomplished = false;
    user_ach.insert_date = *time;
    user_ach.end_date = ach.end_date ? util::datetime_to_timestamp(*ach.end_date) : 0;
    user_ach.is_new = true;
    user_ach.reset_type = ach.reset_type;
    db::main::Achievement *added = context.db.main.add(user_ach);
    if (flush) context.db.main.flush();
    return added;
}

bool has_achievement(Context &context, db::main::User &user, int64_t achievement_id) {
    return context.db.main.scalar_int("SELECT achievement_id FROM achievement WHERE achievement_id = ? AND user_id = ? "
                                      "LIMIT 1",
                                      achievement_id, user.id)
        .has_value();
}

void update_resettable_achievement(Context &context, db::main::User &user, std::optional<int64_t> ts) {
    if (!ts) ts = util::time();

    auto &done = context.memo<std::map<int64_t, bool>>(("update_resettable_achievement_" + std::to_string(*ts)).c_str());
    if (done.count(user.id)) return;

    bool modified = false;
    auto rows = context.db.main.select<db::main::Achievement>(
        "WHERE user_id = ? AND reset_type > 0 AND ((reset_type = 1 AND reset_value != ?) OR "
        "(reset_type = 2 AND reset_value != ?) OR (reset_type = 3 AND reset_value != ?))",
        user.id, util::get_days_since_unix(*ts), util::get_weeks_since_unix(*ts), (int64_t)util::get_months_since_unix(*ts));

    for (auto *ach_data : rows) {
        const Ach *ach_info = context.db.achievement.get<Ach>(ach_data->achievement_id);

        if (ach_info == nullptr || !ach_info->default_open_flag) {
            context.db.main.remove(ach_data);
        } else {
            ach_data->is_accomplished = false;
            ach_data->is_reward_claimed = false;
            ach_data->count = 0;
        }

        modified = true;
    }

    if (modified) context.db.main.flush();

    done[user.id] = true;
}

std::vector<AchievementData> to_game_representation(Context &context, const std::vector<db::main::Achievement *> &achs,
                                                    const std::vector<std::vector<model::Ptr<item_model::Item>>> &rewardss) {
    std::vector<AchievementData> out;
    for (size_t i = 0; i < achs.size() && i < rewardss.size(); i++)
        out.push_back(from_sqlalchemy(*achs[i], *get_achievement_info(context, achs[i]->achievement_id), rewardss[i]));
    return out;
}

std::vector<model::Ptr<item_model::Item>> get_achievement_rewards(Context &context, const db::main::Achievement &ach) {
    return get_achievement_rewards(context, ach.achievement_id);
}

std::vector<model::Ptr<item_model::Item>> get_achievement_rewards(Context &context, int64_t ach_id) {
    const data::ServerData &server_data = data::get();
    std::vector<model::Ptr<item_model::Item>> out;
    if (const auto *old_reward_data = server_data.achievement_reward.find(ach_id)) {
        for (const auto &i : *old_reward_data) out.push_back(advanced::deserialize_item_data(context, *i));
    } else {
        for (const auto &i : ACHIEVEMENT_REWARD_DEFAULT) out.push_back(advanced::deserialize_item_data(context, i));
    }
    return out;
}

void init(Context &context, db::main::User &user) {
    int64_t time = util::time();
    for (const Ach *ach : context.db.achievement.select<Ach>("WHERE default_open_flag = 1")) {
        int64_t start_date = util::datetime_to_timestamp(ach->start_date);
        int64_t end_date = ach->end_date ? util::datetime_to_timestamp(*ach->end_date) : 0;

        if (time >= start_date && (end_date == 0 || time < end_date)) {
            // One flush below instead of one per row: the same INSERTs in the same order, without
            // re-checking every loaded row for changes after each one.
            db::main::Achievement *user_ach = add_achievement(context, user, *ach, time, false);
            if (end_date > 0) user_ach->end_date = end_date;
        }
    }

    context.db.main.flush();
}

db::main::Achievement *get_achievement(Context &context, db::main::User &user, int64_t ach_id) {
    db::main::Achievement *ach = context.db.main.get<db::main::Achievement>(ach_id);
    if (ach == nullptr || ach->user_id != user.id)
        throw error::IdolError(200, 720, "invalid achievement id " + std::to_string(ach_id));
    return ach;
}

std::vector<db::main::Achievement *> get_achievements(Context &context, db::main::User &user,
                                                      std::optional<bool> accomplished) {
    update_resettable_achievement(context, user);

    if (accomplished)
        return context.db.main.select<db::main::Achievement>("WHERE user_id = ? AND is_accomplished = ?", user.id,
                                                             *accomplished);
    return context.db.main.select<db::main::Achievement>("WHERE user_id = ?", user.id);
}

std::vector<db::main::Achievement *> get_unclaimed_achievements(Context &context, db::main::User &user,
                                                                bool accomplished_but_unclaimed_only) {
    if (accomplished_but_unclaimed_only)
        return context.db.main.select<db::main::Achievement>(
            "WHERE user_id = ? AND is_accomplished = 1 AND is_reward_claimed = 0", user.id);
    return context.db.main.select<db::main::Achievement>(
        "WHERE user_id = ? AND (is_accomplished = 0 OR is_reward_claimed = 0)", user.id);
}

std::vector<db::main::Achievement *> get_unclaimed_achievements_by_filter_id(Context &context, db::main::User &user,
                                                                             int64_t achievement_filter_category_id) {
    return context.db.main.select<db::main::Achievement>(
        "WHERE user_id = ? AND achievement_filter_category_id = ? AND (is_accomplished = 0 OR is_reward_claimed = 0)",
        user.id, achievement_filter_category_id);
}

std::vector<db::main::Achievement *> get_accomplished_achievements_by_filter_id(Context &context, db::main::User &user,
                                                                                int64_t achievement_filter_category_id) {
    return context.db.main.select<db::main::Achievement>(
        "WHERE user_id = ? AND achievement_filter_category_id = ? AND is_accomplished = 1", user.id,
        achievement_filter_category_id);
}

void mark_achievement_reward_claimed(Context &, db::main::Achievement &ach) { ach.is_reward_claimed = true; }

int64_t get_achievement_count(Context &context, db::main::User &user, std::optional<bool> accomplished) {
    if (accomplished)
        return context.db.main.count<db::main::Achievement>("WHERE user_id = ? AND is_accomplished = ?", user.id,
                                                            *accomplished);
    return context.db.main.count<db::main::Achievement>("WHERE user_id = ?", user.id);
}

bool is_unlock_satisfied(Context &context, int64_t target_ach_id, const std::vector<int64_t> &unlocked_ach_ids) {
    std::set<int64_t> ach_ids(unlocked_ach_ids.begin(), unlocked_ach_ids.end());
    std::vector<int64_t> prereq_list = get_prerequisite_achievement_ids(context, target_ach_id);
    std::set<int64_t> prerequisite(prereq_list.begin(), prereq_list.end());
    for (int64_t p : prerequisite)
        if (!ach_ids.count(p)) return false;
    return true;
}

std::vector<int64_t> get_unlocked_achievements_by_id(Context &context, db::main::User &target_user,
                                                     const std::vector<int64_t> &achievement_ids) {
    std::vector<int64_t> out;
    if (achievement_ids.empty()) return out;
    db::Stmt &s = context.db.main.query("SELECT achievement_id FROM achievement WHERE user_id = ? AND achievement_id IN " +
                                        placeholders(achievement_ids.size()) + " AND is_accomplished = 1");
    s.bind(1, target_user.id);
    for (size_t i = 0; i < achievement_ids.size(); i++) s.bind((int)i + 2, achievement_ids[i]);
    while (s.step()) out.push_back(s.get_int(0));
    s.reset();
    return out;
}

// ---------------------------------------------------------------- checkers

static const std::optional<int64_t> &P(const Ach &a, int n) {
    switch (n) {
    case 1: return a.params1;
    case 2: return a.params2;
    case 3: return a.params3;
    case 4: return a.params4;
    case 5: return a.params5;
    case 6: return a.params6;
    case 7: return a.params7;
    case 8: return a.params8;
    case 9: return a.params9;
    case 10: return a.params10;
    default: return a.params11;
    }
}

// assert x is not None
static int64_t need(const std::optional<int64_t> &v) {
    if (!v) throw std::logic_error("assertion failed: achievement parameter is None");
    return *v;
}

struct Checker {
    std::function<bool(Context &, const AchievementUpdate &, const Ach &)> test_param;
    std::function<int64_t(const Ach &)> maxvalue;
    std::function<int64_t(Context &, db::main::User &, const Ach &)> initvalue;
    std::function<int64_t(Context &, db::main::User &, int64_t, const AchievementUpdate &, const Ach &)> update;
    std::function<bool(Context &, int64_t, const Ach &)> is_accomplished;
    bool recursive = false;
};

template <typename T, size_t I = 0> constexpr size_t variant_index() {
    if constexpr (std::is_same_v<std::variant_alternative_t<I, AchievementUpdate>, T>) return I;
    else return variant_index<T, I + 1>();
}

template <typename T> struct Def {
    std::function<bool(Context &, const T &, const Ach &)> test_param;
    std::function<int64_t(const Ach &)> maxvalue;
    std::function<bool(Context &, int64_t, const Ach &)> is_accomplished;
    bool recursive = false;
    std::function<int64_t(Context &, db::main::User &, const Ach &)> initvalue = nullptr;
    std::function<int64_t(Context &, db::main::User &, int64_t, const T &, const Ach &)> update = nullptr;
};

using Registry = std::array<std::map<int64_t, Checker>, std::variant_size_v<AchievementUpdate>>;

template <typename T> static void reg(Registry &r, std::initializer_list<int64_t> ids, Def<T> d) {
    Checker c;
    c.test_param = [f = d.test_param](Context &ctx, const AchievementUpdate &u, const Ach &a) {
        return f(ctx, std::get<T>(u), a);
    };
    c.maxvalue = d.maxvalue;
    c.is_accomplished = d.is_accomplished;
    c.recursive = d.recursive;
    c.initvalue = d.initvalue ? d.initvalue : [](Context &, db::main::User &, const Ach &) -> int64_t { return 0; };
    if (d.update)
        c.update = [f = d.update](Context &ctx, db::main::User &u, int64_t old, const AchievementUpdate &up,
                                  const Ach &a) { return f(ctx, u, old, std::get<T>(up), a); };
    else
        c.update = [](Context &, db::main::User &, int64_t old, const AchievementUpdate &, const Ach &) {
            return old + 1;
        };
    for (int64_t id : ids) r[variant_index<T>()][id] = c;
}

// maxvalue / is_accomplished from params n
static std::function<int64_t(const Ach &)> max_p(int n) {
    return [n](const Ach &a) { return need(P(a, n)); };
}
static std::function<bool(Context &, int64_t, const Ach &)> acc_p(int n) {
    return [n](Context &, int64_t v, const Ach &a) { return v >= need(P(a, n)); };
}
static const auto max_one = [](const Ach &) -> int64_t { return 1; };
static const auto acc_positive = [](Context &, int64_t v, const Ach &) { return v > 0; };

static std::vector<int64_t> unit_type_ids_of(Context &context, const std::vector<int64_t> &unit_ids) {
    std::vector<int64_t> out;
    for (int64_t unit_id : unit_ids) {
        out.push_back(unit::get_unit_info(context, unit_id)->unit_type_id);
    }
    return out;
}

static std::set<int64_t> id_set(const std::vector<int64_t> &v) { return {v.begin(), v.end()}; }

static Registry build_registry() {
    Registry r;
    using LC = AchievementUpdateLiveClear;
    using UM = AchievementUpdateUnitMerge;
    using RU = AchievementUpdateUnitRankUp;
    using LB = AchievementUpdateLoginBonus;
    using FS = AchievementUpdateFinishScenario;
    using IC = AchievementUpdateItemCollect;

    reg<LC>(r, {1}, {.test_param = [](Context &, const LC &, const Ach &a) { return a.params1.has_value(); },
                     .maxvalue = max_p(1), .is_accomplished = acc_p(1)});
    reg<LC>(r, {2}, {.test_param = [](Context &, const LC &d, const Ach &a) {
                         return a.params1.has_value() && d.difficulty == *a.params1;
                     },
                     .maxvalue = max_p(2), .is_accomplished = acc_p(2)});
    reg<LC>(r, {3}, {.test_param = [](Context &, const LC &d, const Ach &a) {
                         return a.params1.has_value() && d.score_rank <= *a.params1;
                     },
                     .maxvalue = max_p(2), .is_accomplished = acc_p(2)});
    reg<LC>(r, {4}, {.test_param = [](Context &, const LC &d, const Ach &a) {
                         return a.params1.has_value() && d.combo_rank <= *a.params1;
                     },
                     .maxvalue = max_p(2), .is_accomplished = acc_p(2)});
    reg<LC>(r, {7}, {.test_param = [](Context &, const LC &d, const Ach &a) {
                         return a.params1.has_value() &&
                                std::find(d.team_unit_type_ids.begin(), d.team_unit_type_ids.end(), *a.params1) !=
                                    d.team_unit_type_ids.end();
                     },
                     .maxvalue = max_p(2), .is_accomplished = acc_p(2)});
    reg<LC>(r, {9}, {.test_param =
                         [](Context &ctx, const LC &d, const Ach &a) {
                             if (!a.params1 || !a.params2 || !a.params3) return false;
                             if (d.live_track_id != *a.params1) return false;

                             auto groups = get_unit_type_groups(ctx, *a.params2);
                             std::set<int64_t> group_set;
                             for (const auto &k : groups) group_set.insert(std::get<0>(k));
                             std::set<int64_t> team_set(d.team_unit_type_ids.begin(), d.team_unit_type_ids.end());

                             switch (*a.params3) {
                             case 1:
                                 for (int64_t g : group_set)
                                     if (!team_set.count(g)) return false;
                                 return true;
                             case 2:
                                 for (int64_t t : d.team_unit_type_ids)
                                     if (group_set.count(t)) return true;
                                 return false;
                             case 3:
                                 for (int64_t t : team_set)
                                     if (!group_set.count(t)) return false;
                                 return true;
                             default: return false;
                             }
                         },
                     .maxvalue = max_one, .is_accomplished = acc_positive});
    reg<AchievementUpdateSecretbox>(
        r, {10},
        {.test_param =
             [](Context &, const AchievementUpdateSecretbox &d, const Ach &a) {
                 if (!a.params1 || !a.params3) return false;
                 const data::ServerData &npps4_data = data::get();
                 if (const auto *sb_info = npps4_data.secretbox_data.find(d.secretbox_id)) {
                     if (sb_info->achievement_secretbox_id > 0 && sb_info->achievement_secretbox_id == *a.params1)
                         return true;
                 }
                 return *a.params1 == d.secretbox_id;
             },
         .maxvalue = max_p(3), .is_accomplished = acc_p(3), .recursive = true,
         .update = [](Context &, db::main::User &, int64_t old, const AchievementUpdateSecretbox &d,
                      const Ach &) { return old + d.amount; }});
    reg<UM>(r, {11}, {.test_param = [](Context &, const UM &, const Ach &a) { return a.params1.has_value(); },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
                      .update = [](Context &, db::main::User &, int64_t old, const UM &d, const Ach &) {
                          return old + d.amount;
                      }});
    reg<UM>(r, {13}, {.test_param = [](Context &, const UM &, const Ach &a) { return a.params1.has_value(); },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
                      .update = [](Context &, db::main::User &, int64_t, const UM &d, const Ach &) {
                          return d.skill_level;
                      }});
    auto album_checker = [](const char *criteria) {
        return [criteria](Context &ctx, db::main::User &u, int64_t, const auto &, const Ach &) {
            return album::count_album_with(ctx, u, criteria);
        };
    };
    reg<AchievementUpdateNewUnit>(
        r, {18},
        {.test_param = [](Context &, const AchievementUpdateNewUnit &, const Ach &a) { return a.params1.has_value(); },
         .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
         .update = album_checker("")});
    reg<RU>(r, {19}, {.test_param = [](Context &, const RU &, const Ach &a) { return a.params1.has_value(); },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
                      .update = album_checker("rank_max_flag = 1")});
    reg<AchievementUpdateUnitMaxLove>(
        r, {20},
        {.test_param = [](Context &, const AchievementUpdateUnitMaxLove &, const Ach &a) {
             return a.params1.has_value();
         },
         .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
         .update = album_checker("love_max_flag = 1")});
    reg<AchievementUpdateUnitMaxLevel>(
        r, {21},
        {.test_param = [](Context &, const AchievementUpdateUnitMaxLevel &, const Ach &a) {
             return a.params1.has_value();
         },
         .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
         .update = album_checker("rank_level_max_flag = 1")});
    reg<FS>(r, {23}, {.test_param = [](Context &, const FS &d, const Ach &a) {
                          return a.params1.has_value() && *a.params1 == d.scenario_id;
                      },
                      .maxvalue = max_one, .is_accomplished = acc_positive});
    reg<AchievementUpdateAnywhere>(
        r, {26},
        {.test_param = [](Context &, const AchievementUpdateAnywhere &, const Ach &a) { return a.params1.has_value(); },
         .maxvalue = max_p(1),
         // TODO: query friend count
         .is_accomplished = [](Context &, int64_t, const Ach &) { return false; }, .recursive = true,
         .update = [](Context &, db::main::User &, int64_t old, const AchievementUpdateAnywhere &, const Ach &) {
             return old;
         }});
    reg<LB>(r, {27}, {.test_param = [](Context &, const LB &, const Ach &a) { return a.params1.has_value(); },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1)});
    reg<LB>(r, {29}, {.test_param = [](Context &, const LB &, const Ach &) { return true; },
                      .maxvalue = max_one, .is_accomplished = acc_positive});
    reg<AchievementUpdateLevelUp>(
        r, {30},
        {.test_param = [](Context &, const AchievementUpdateLevelUp &, const Ach &a) { return a.params1.has_value(); },
         .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
         .initvalue = [](Context &, db::main::User &u, const Ach &) { return u.level; },
         .update = [](Context &, db::main::User &, int64_t, const AchievementUpdateLevelUp &d, const Ach &) {
             return d.rank;
         }});
    reg<LC>(r, {32, 37},
            {.test_param = [](Context &, const LC &d, const Ach &a) {
                 return a.params1.has_value() && *a.params1 == d.live_track_id;
             },
             .maxvalue = [](const Ach &a) { return a.params2 && *a.params2 ? *a.params2 : (int64_t)1; },
             .is_accomplished = [](Context &, int64_t v, const Ach &a) {
                 int64_t amount = a.params2 && *a.params2 ? *a.params2 : 1;
                 return v >= amount;
             }});
    reg<RU>(r, {33},
            {.test_param =
                 [](Context &ctx, const RU &d, const Ach &a) {
                     if (!a.params1 || !a.params2) return false;
                     auto ids = unit_type_ids_of(ctx, d.unit_ids);
                     return std::find(ids.begin(), ids.end(), *a.params1) != ids.end();
                 },
             .maxvalue = max_p(2), .is_accomplished = acc_p(2),
             .update = [](Context &ctx, db::main::User &, int64_t old, const RU &d, const Ach &a) {
                 int64_t p1 = need(a.params1);
                 auto ids = unit_type_ids_of(ctx, d.unit_ids);
                 return old + (int64_t)std::count(ids.begin(), ids.end(), p1);
             }});
    reg<RU>(r, {38}, {.test_param = [](Context &, const RU &d, const Ach &a) {
                           return !a.params1.has_value() && !d.unit_ids.empty();
                       },
                       .maxvalue = max_p(1), .is_accomplished = acc_p(1),
                       .update = [](Context &, db::main::User &, int64_t old, const RU &d, const Ach &) {
                           return old + (int64_t)d.unit_ids.size();
                       }});
    reg<LC>(r, {50},
            {.test_param =
                 [](Context &ctx, const LC &d, const Ach &a) {
                     if (!a.params10) return false;
                     // Check live track
                     if (a.params1 && *a.params1 != d.live_track_id) return false;
                     // Check difficulty
                     if (a.params2 && *a.params2 != d.difficulty) return false;
                     // Check attribute
                     if (a.params3 && *a.params3 != d.attribute) return false;
                     // Check score rank
                     if (a.params5 && d.score_rank > *a.params5) return false;
                     // Check combo rank
                     if (a.params6 && d.combo_rank > *a.params6) return false;
                     // Check swing constraint
                     if (a.params11 && (*a.params11 != 0) != d.swing) return false;

                     // Check unit constraints
                     if (a.params8) {
                         if (!a.params7 || !a.params9) return false;

                         auto unit_type_group = get_unit_type_groups(ctx, *a.params8);
                         std::vector<int64_t> unit_type_group_ids;
                         for (const auto &x : unit_type_group) unit_type_group_ids.push_back(std::get<0>(x));
                         std::set<int64_t> group_set(unit_type_group_ids.begin(), unit_type_group_ids.end());

                         // Test params7
                         std::set<int64_t> params7_data;
                         for (int64_t t : d.team_unit_type_ids)
                             if (group_set.count(t)) params7_data.insert(t);
                         bool params7_ok;
                         switch (*a.params7) {
                         case 1: params7_ok = params7_data.size() == 9; break;
                         case 2: params7_ok = !params7_data.empty(); break;
                         default: params7_ok = false;
                         }
                         if (!params7_ok) return false;

                         // Test params9
                         std::set<int64_t> params9_data = group_set;
                         for (int64_t t : d.team_unit_type_ids) params9_data.erase(t);
                         bool params9_ok;
                         switch (*a.params9) {
                         case 1: params9_ok = params9_data.empty(); break;
                         case 2: params9_ok = params9_data.size() != unit_type_group_ids.size(); break;
                         default: params9_ok = false;
                         }
                         if (!params9_ok) return false;
                     }
                     return true;
                 },
             .maxvalue = max_p(10), .is_accomplished = acc_p(10)});
    reg<LC>(r, {51}, {.test_param = [](Context &, const LC &d, const Ach &a) {
                          if (a.params1 != std::optional<int64_t>(1) || !a.params2 || !a.params3) return false;
                          return std::find(d.team_unit_type_ids.begin(), d.team_unit_type_ids.end(), *a.params2) !=
                                 d.team_unit_type_ids.end();
                      },
                      .maxvalue = max_p(3), .is_accomplished = acc_p(3)});
    reg<LB>(r, {52}, {.test_param = [](Context &, const LB &, const Ach &a) { return a.params1.has_value(); },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true});
    reg<AchievementUpdateAchievementComplete>(
        r, {53},
        {.test_param =
             [](Context &ctx, const AchievementUpdateAchievementComplete &d, const Ach &a) {
                 if (!a.params1 || !a.params2) return false;
                 auto ach_id_in_cat = id_set(get_achievement_ids_from_category(ctx, *a.params1));
                 for (int64_t ach_id : d.completed_achievement_id)
                     if (ach_id_in_cat.count(ach_id)) return true;
                 return false;
             },
         .maxvalue = max_p(2), .is_accomplished = acc_p(2), .recursive = true,
         .initvalue =
             [](Context &ctx, db::main::User &u, const Ach &a) {
                 auto ach_id_in_cat = id_set(get_achievement_ids_from_category(ctx, need(a.params1)));
                 return count_accomplished_achievement_by_set(ctx, u, ach_id_in_cat);
             },
         .update = [](Context &ctx, db::main::User &u, int64_t, const AchievementUpdateAchievementComplete &d,
                      const Ach &a) {
             auto ach_id_in_cat = id_set(get_achievement_ids_from_category(ctx, need(a.params1)));
             auto completed_ach_id = id_set(d.completed_achievement_id);
             int64_t total_ach = count_accomplished_achievement_by_set(ctx, u, ach_id_in_cat);
             int64_t inter = 0;
             for (int64_t x : ach_id_in_cat)
                 if (completed_ach_id.count(x)) inter++;
             return total_ach + inter;
         }});
    reg<IC>(r, {55},
            {.test_param =
                 [](Context &, const IC &d, const Ach &a) {
                     if (d.add_type != const_::ADD_TYPE::ITEM || !a.params1 || !a.params3) return false;
                     return d.item_id == *a.params1;
                 },
             .maxvalue = max_p(1), .is_accomplished = acc_p(3), .recursive = true,
             .initvalue = [](Context &ctx, db::main::User &u, const Ach &a) {
                 return item::get_item_count(ctx, u, need(a.params1));
             },
             .update = [](Context &ctx, db::main::User &u, int64_t, const IC &d, const Ach &) {
                 return item::get_item_count(ctx, u, d.item_id);
             }});
    reg<FS>(r, {57}, {.test_param = [](Context &, const FS &, const Ach &a) { return a.params1.has_value(); },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true});
    reg<LC>(r, {58},
            {.test_param = [](Context &, const LC &, const Ach &a) { return a.params1.has_value(); },
             .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true,
             .initvalue = [](Context &ctx, db::main::User &u, const Ach &) {
                 return ctx.db.main
                     .scalar_int("SELECT count FROM achievement WHERE user_id = ? AND achievement_type = 58 "
                                 "ORDER BY count DESC LIMIT 1",
                                 u.id)
                     .value_or(0);
             }});
    reg<IC>(r, {59}, {.test_param = [](Context &, const IC &d, const Ach &a) {
                          return d.add_type == const_::ADD_TYPE::SCENARIO && a.params1.has_value();
                      },
                      .maxvalue = max_p(1), .is_accomplished = acc_p(1), .recursive = true});
    reg<LB>(r, {60}, {.test_param = [](Context &, const LB &, const Ach &a) {
                          if (!a.params1 || !a.params2) return false;
                          auto dt = util::datetime(util::time());
                          return *a.params1 == dt.month && *a.params2 == dt.day;
                      },
                      .maxvalue = max_one, .is_accomplished = acc_positive});
    return r;
}

static const Registry &registry() {
    static const Registry r = build_registry();
    return r;
}

static void check_impl(Context &context, db::main::User &target_user, const AchievementUpdate &update_instance,
                       AchievementContext &container) {
    const auto &info = registry()[update_instance.index()];
    if (info.empty()) {
        util::log("Achievement updater #%llu does not exist. This is probably not what you want.",
                  (unsigned long long)update_instance.index());
        return;
    }

    std::set<int64_t> acceptable_achievement_type;
    for (const auto &kv : info) acceptable_achievement_type.insert(kv.first);
    std::string in_list;
    for (int64_t t : acceptable_achievement_type) in_list += (in_list.empty() ? "" : ", ") + std::to_string(t);
    // for recursive checkers; pop() takes from the end
    std::vector<db::main::Achievement *> queue = context.db.main.select<db::main::Achievement>(
        "WHERE user_id = ? AND achievement_type IN (" + in_list + ") AND is_accomplished = 0", target_user.id);
    for (auto *c : container.new_)
        if (acceptable_achievement_type.count(c->achievement_type)) queue.push_back(c);
    std::set<int64_t> newly_added;

    while (!queue.empty()) {
        db::main::Achievement *ach = queue.back();
        queue.pop_back();

        if (ach->is_accomplished) {
            // Likely a duplicate
            continue;
        }

        const Ach *ach_info = get_achievement_info(context, ach->achievement_id);
        auto cit = info.find(ach_info->achievement_type);
        const Checker *checker = cit == info.end() ? nullptr : &cit->second;

        if (checker != nullptr && checker->test_param(context, update_instance, *ach_info)) {
            if (!newly_added.count(ach->achievement_id))
                ach->count = checker->update(context, target_user, ach->count, update_instance, *ach_info);

            if (checker->is_accomplished(context, ach->count, *ach_info)) {
                // Accomplished
                ach->is_accomplished = true;
                container.add(ach);  // Add to accomplished list

                // Get next achievement
                for (int64_t open_ach_id : get_next_achievement_ids(context, ach->achievement_id)) {
                    std::vector<int64_t> unlocked;
                    for (const auto *a : container.accomplished) unlocked.push_back(a->achievement_id);
                    auto from_db = get_unlocked_achievements_by_id(
                        context, target_user, get_prerequisite_achievement_ids(context, open_ach_id));
                    unlocked.insert(unlocked.end(), from_db.begin(), from_db.end());
                    if (is_unlock_satisfied(context, open_ach_id, unlocked) &&
                        !has_achievement(context, target_user, open_ach_id)) {
                        const Ach *new_ach_info = get_achievement_info(context, open_ach_id);
                        auto *new_ach = add_achievement(context, target_user, *new_ach_info, std::nullopt, false);
                        container.add(new_ach);

                        if (checker->recursive) {
                            if (ach_info->achievement_type == new_ach_info->achievement_type) {
                                // Carryover value
                                new_ach->count = ach->count;
                            } else {
                                new_ach->count = checker->initvalue(context, target_user, *new_ach_info);
                            }
                        } else {
                            new_ach->count = checker->initvalue(context, target_user, *new_ach_info);
                        }

                        if (new_ach->count > 0 || checker->recursive) {
                            // Assume recursive check
                            queue.push_back(new_ach);
                            newly_added.insert(new_ach->achievement_id);
                        }
                    }
                }

                // Clamp
                ach->count = std::min(ach->count, checker->maxvalue(*ach_info));
            }
        }

        if (ach->is_accomplished && newly_added.count(ach->achievement_id)) newly_added.erase(ach->achievement_id);
    }
}

AchievementContext check(Context &context, db::main::User &target_user, const std::vector<AchievementUpdate> &updates) {
    AchievementContext container;

    for (const auto &update_instance : updates) check_impl(context, target_user, update_instance, container);

    // Anywhere check
    check_impl(context, target_user, AchievementUpdateAnywhere{}, container);

    // Type 53 check
    container.fix();
    AchievementContext type_53_container;
    AchievementUpdateAchievementComplete complete;
    for (const auto *a : container.accomplished) complete.completed_achievement_id.push_back(a->achievement_id);
    check_impl(context, target_user, complete, type_53_container);

    context.db.main.flush();
    return container + type_53_container;
}

std::vector<int64_t> get_achievement_filter_ids(Context &context) {
    std::vector<int64_t> out;
    for (const auto *f : context.db.achievement.select<db::achievement::FilterCategory>(""))
        out.push_back(f->achievement_filter_category_id);
    return out;
}

std::vector<std::tuple<int64_t, int64_t>> count_accomplished_achievement_by_category(Context &context,
                                                                                    db::main::User &user) {
    // Get all achieved
    std::vector<int64_t> all_accomplished;
    {
        db::Stmt &s = context.db.main.query("SELECT achievement_id FROM achievement WHERE user_id = ? AND "
                                            "is_accomplished = 1");
        s.bind(1, user.id);
        while (s.step()) all_accomplished.push_back(s.get_int(0));
        s.reset();
    }

    std::vector<std::tuple<int64_t, int64_t>> out;
    if (all_accomplished.empty()) return out;
    db::Stmt &s = context.db.achievement.query(
        "SELECT achievement_category_id, COUNT(DISTINCT achievement_id) FROM achievement_tag_m WHERE achievement_id IN " +
        placeholders(all_accomplished.size()) + " GROUP BY achievement_category_id");
    for (size_t i = 0; i < all_accomplished.size(); i++) s.bind((int)i + 1, all_accomplished[i]);
    while (s.step()) out.emplace_back(s.get_int(0), s.get_int(1));
    s.reset();
    return out;
}

int64_t count_accomplished_achievement_by_set(Context &context, db::main::User &user, const std::set<int64_t> &ach_ids) {
    if (ach_ids.empty()) return 0;
    db::Stmt &s = context.db.main.query("SELECT COUNT(*) FROM achievement WHERE user_id = ? AND is_accomplished = 1 AND "
                                        "achievement_id IN " +
                                        placeholders(ach_ids.size()));
    s.bind(1, user.id);
    int i = 2;
    for (int64_t id : ach_ids) s.bind(i++, id);
    int64_t n = 0;
    if (s.step()) n = s.get_int(0);
    s.reset();
    return n;
}

void give_achievement_reward(Context &context, db::main::User &user, const Ach &ach_info,
                             const std::vector<model::Ptr<item_model::Item>> &rewards) {
    auto title = [&]() { return ach_info.title && !ach_info.title->empty() ? *ach_info.title : std::string("FIXME"); };
    auto title_en = [&]() {
        if (ach_info.title_en && !ach_info.title_en->empty()) return *ach_info.title_en;
        if (ach_info.title && !ach_info.title->empty()) return *ach_info.title;
        return std::string("FIXME EN");
    };
    for (const auto &r : rewards) {
        // TODO: Proper message for reward insertion
        if (r->reward_box_flag) {
            reward::add_item(context, user, r, title(), title_en());
        } else {
            auto add_result = advanced::add_item(context, user, r);
            if (!add_result.success) reward::add_item(context, user, r, title(), title_en());
        }
    }
}

void process_achievement_reward(Context &context, db::main::User &user,
                                const std::vector<db::main::Achievement *> &achievements,
                                const std::vector<std::vector<model::Ptr<item_model::Item>>> &rewardss) {
    for (size_t i = 0; i < achievements.size() && i < rewardss.size(); i++) {
        auto *ach = achievements[i];
        const auto &reward_list = rewardss[i];
        const Ach *ach_info = get_achievement_info(context, ach->achievement_id);
        if (ach_info->auto_reward_flag) {
            give_achievement_reward(context, user, *ach_info, reward_list);
            mark_achievement_reward_claimed(context, *ach);
        } else {
            // Backgrounds and titles are always gained, regardless of the auto_reward_flag
            for (const auto &r : reward_list)
                if (std::find(BACKGROUND_TITLE_ADD_TYPE.begin(), BACKGROUND_TITLE_ADD_TYPE.end(), r->add_type) !=
                    BACKGROUND_TITLE_ADD_TYPE.end())
                    advanced::add_item(context, user, r);
        }
    }
    context.db.main.flush();
}

}  // namespace npps::system::achievement
