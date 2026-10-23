// npps4/system/live.py
#include "system/live.hpp"

#include "app/config.hpp"
#include "core/crypto.hpp"
#include "external/beatmap.hpp"

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace npps::system::live {

namespace {

// gzip.compress / gzip.decompress
Bytes gzip_compress(const std::string &data) {
    z_stream zs{};
    if (deflateInit2(&zs, 9, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) throw std::runtime_error("deflateInit2");
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = (uInt)data.size();
    std::string out;
    char buf[16384];
    int rc;
    do {
        zs.next_out = (Bytef *)buf;
        zs.avail_out = sizeof(buf);
        rc = deflate(&zs, Z_FINISH);
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (rc == Z_OK);
    deflateEnd(&zs);
    if (rc != Z_STREAM_END) throw std::runtime_error("gzip compress failed");
    return out;
}

std::string gzip_decompress(const Bytes &data) {
    z_stream zs{};
    if (inflateInit2(&zs, 47) != Z_OK) throw std::runtime_error("inflateInit2");
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = (uInt)data.size();
    std::string out;
    char buf[16384];
    int rc;
    do {
        zs.next_out = (Bytef *)buf;
        zs.avail_out = sizeof(buf);
        rc = inflate(&zs, Z_NO_FLUSH);
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (rc == Z_OK);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) throw std::runtime_error("gzip decompress failed");
    return out;
}

// Comma separated integers for an IN (...) clause.
std::string in_list(const std::vector<int64_t> &values) {
    std::string out;
    for (size_t i = 0; i < values.size(); i++) {
        if (i) out += ',';
        out += std::to_string(values[i]);
    }
    return out;
}

// Master tables have no index on the columns live status filters by, so a
// per-track query scans the whole table. These maps are built with one scan
// each; values keep their scan position so lookups return rows in the order
// the filtered SELECT would.
struct ScanIndex {
    std::unordered_map<int64_t, std::vector<std::pair<size_t, int64_t>>> rows;
};

ScanIndex build_scan_index(Context &context, const std::string &sql) {
    ScanIndex index;
    db::Stmt &s = context.db.live.query(sql);
    for (size_t position = 0; s.step(); position++) index.rows[s.get_int(0)].emplace_back(position, s.get_int(1));
    s.reset();
    return index;
}

// Equivalent to "SELECT value FROM ... WHERE key IN (keys)".
std::vector<int64_t> scan_lookup(const ScanIndex &index, const std::vector<int64_t> &keys) {
    std::set<int64_t> unique(keys.begin(), keys.end());
    std::vector<std::pair<size_t, int64_t>> found;
    for (int64_t key : unique) {
        auto it = index.rows.find(key);
        if (it != index.rows.end()) found.insert(found.end(), it->second.begin(), it->second.end());
    }
    std::sort(found.begin(), found.end());
    std::vector<int64_t> out;
    out.reserve(found.size());
    for (auto &f : found) out.push_back(f.second);
    return out;
}

const ScanIndex &normal_live_by_setting(Context &context) {
    static const ScanIndex index = build_scan_index(
        context, std::string("SELECT live_setting_id, live_difficulty_id FROM ") + db::live::NormalLive::TABLE);
    return index;
}

std::vector<int64_t> read_ints(db::Stmt &s) {
    std::vector<int64_t> out;
    while (s.step()) out.push_back(s.get_int(0));
    s.reset();
    return out;
}

LiveNote note_from_beatmap(const external::beatmap::BeatmapData &l) {
    LiveNote note;
    note.timing_sec = l.timing_sec;
    note.notes_attribute = l.notes_attribute;
    note.notes_level = l.notes_level;
    note.effect = l.effect;
    note.effect_value = l.effect_value;
    note.position = l.position;
    note.speed = l.speed;
    note.vanish = l.vanish;
    return note;
}

std::vector<LiveNote> notes_from_beatmap(const std::vector<external::beatmap::BeatmapData> &beatmap_data) {
    std::vector<LiveNote> notes;
    notes.reserve(beatmap_data.size());
    for (const auto &l : beatmap_data) notes.push_back(note_from_beatmap(l));
    return notes;
}

}  // namespace

// Pydantic (Rust serde_json) compact output: no spaces, exponents without "+" or leading zeros.
static void compact_dump(std::string &out, const Json &j) {
    switch (j.type()) {
    case Json::Array: {
        out += '[';
        bool first = true;
        for (const Json &e : j.elements()) {
            if (!first) out += ',';
            first = false;
            compact_dump(out, e);
        }
        out += ']';
        break;
    }
    case Json::Object: {
        out += '{';
        bool first = true;
        for (const auto &[k, v] : j.members()) {
            if (!first) out += ',';
            first = false;
            json_escape_to(out, k);
            out += ':';
            compact_dump(out, v);
        }
        out += '}';
        break;
    }
    case Json::Double: {
        std::string text;
        json_double_to(text, j.as_double());
        size_t e = text.find('e');
        if (e != std::string::npos) {
            std::string mant = text.substr(0, e), exp = text.substr(e + 1);
            bool neg = !exp.empty() && exp[0] == '-';
            if (!exp.empty() && (exp[0] == '-' || exp[0] == '+')) exp.erase(0, 1);
            size_t nz = exp.find_first_not_of('0');
            exp = nz == std::string::npos ? "0" : exp.substr(nz);
            text = mant + "e" + (neg ? "-" : "") + exp;
        }
        out += text;
        break;
    }
    default:
        j.dump_to(out);
    }
}


bool unlock_normal_live(Context &context, db::main::User &user, int64_t live_track_id) {
    db::main::NormalLiveUnlock *row = context.db.main.first<db::main::NormalLiveUnlock>(
        "WHERE user_id = ? AND live_track_id = ?", user.id, live_track_id);
    if (row == nullptr) {
        db::main::NormalLiveUnlock unlock;
        unlock.user_id = user.id;
        unlock.live_track_id = live_track_id;
        context.db.main.add(unlock);
        context.db.main.flush();
        return true;
    }
    return false;
}

void init(Context &context, db::main::User &user) {
    unlock_normal_live(context, user, 1);  // Bokura no LIVE Kimi to no LIFE

    // Unlock the rest of the live shows.
    std::set<int64_t> unlocked;
    for (const db::live::NormalLive *normallive :
         context.db.live.select<db::live::NormalLive>("WHERE default_unlocked_flag = 1")) {
        const db::live::LiveSetting *setting_data = context.db.live.get<db::live::LiveSetting>(normallive->live_setting_id);
        if (setting_data == nullptr) throw std::logic_error("setting_data is not None");
        if (!unlocked.count(setting_data->live_track_id)) {
            if (unlock_normal_live(context, user, setting_data->live_track_id)) unlocked.insert(setting_data->live_track_id);
        }
    }

    context.db.main.flush();
}

db::main::LiveClear *get_live_clear_data(Context &context, db::main::User &user, int64_t live_difficulty_id,
                                         bool ensure) {
    db::main::LiveClear *live_clear = context.db.main.first<db::main::LiveClear>(
        "WHERE user_id = ? AND live_difficulty_id = ?", user.id, live_difficulty_id);

    if (ensure && live_clear == nullptr) {
        const db::live::LiveSetting *live_setting_info = get_live_setting_from_difficulty_id(context, live_difficulty_id);
        if (live_setting_info == nullptr) throw std::invalid_argument("invalid live_difficulty_id");

        db::main::LiveClear row;
        row.user_id = user.id;
        row.live_difficulty_id = live_difficulty_id;
        row.difficulty = live_setting_info->difficulty;
        live_clear = context.db.main.add(row);
        context.db.main.flush();
    }

    return live_clear;
}

std::vector<db::main::LiveClear *> get_all_live_clear_data(Context &context, db::main::User &user) {
    return context.db.main.select<db::main::LiveClear>("WHERE user_id = ?", user.id);
}

LiveStatus live_status_from_live_clear(Context &context, int64_t live_difficulty_id, db::main::LiveClear *live_clear,
                                       int64_t missing_status) {
    LiveStatus status;
    status.live_difficulty_id = live_difficulty_id;
    if (live_clear == nullptr) {
        status.status = missing_status;
        status.hi_score = 0;
        status.hi_combo_count = 0;
        status.clear_cnt = 0;
        status.achieved_goal_id_list = {};
    } else {
        status.status = 1 + (live_clear->clear_cnt > 0);
        status.hi_score = live_clear->hi_score;
        status.hi_combo_count = live_clear->hi_combo_cnt;
        status.clear_cnt = live_clear->clear_cnt;
        status.achieved_goal_id_list = get_achieved_goal_id_list(context, *live_clear);
    }
    return status;
}

std::vector<LiveStatus> get_normal_live_clear_status(Context &context, db::main::User &user) {
    std::vector<int64_t> track_ids = get_all_normal_live_unlock(context, user);
    std::vector<LiveStatus> live_status_result;

    for (int64_t live_track_id : track_ids) {
        std::vector<int64_t> live_setting_ids = get_live_setting_ids_from_track_id(context, live_track_id);
        if (live_setting_ids.empty()) continue;

        for (int64_t live_difficulty_id : scan_lookup(normal_live_by_setting(context), live_setting_ids)) {
            db::main::LiveClear *live_clear = get_live_clear_data(context, user, live_difficulty_id);
            live_status_result.push_back(live_status_from_live_clear(context, live_difficulty_id, live_clear));
        }
    }

    return live_status_result;
}

std::vector<LiveStatus> get_normal_live_clear_status_of_track(Context &context, db::main::User &user,
                                                              int64_t live_track_id) {
    std::vector<LiveStatus> live_status_result;
    std::vector<int64_t> live_setting_ids = get_live_setting_ids_from_track_id(context, live_track_id);
    if (live_setting_ids.empty()) return live_status_result;

    std::vector<int64_t> live_difficulty_ids = scan_lookup(normal_live_by_setting(context), live_setting_ids);

    for (int64_t live_difficulty_id : live_difficulty_ids) {
        db::main::LiveClear *live_clear = get_live_clear_data(context, user, live_difficulty_id);
        live_status_result.push_back(live_status_from_live_clear(context, live_difficulty_id, live_clear));
    }

    return live_status_result;
}

const CommonLive *get_live_info_table(Context &context, int64_t live_difficulty_id) {
    // @context_cacheable("live_info"): master data only, so a process-wide cache.
    static std::map<int64_t, CommonLive> cache;
    auto it = cache.find(live_difficulty_id);
    if (it != cache.end()) return &it->second;

    CommonLive common;
    if (const db::live::SpecialLive *special = context.db.live.get<db::live::SpecialLive>(live_difficulty_id)) {
        common.special = special;
        common.live_difficulty_id = special->live_difficulty_id;
        common.live_setting_id = special->live_setting_id;
        common.capital_type = special->capital_type;
        common.capital_value = special->capital_value;
        common.c_rank_complete = special->c_rank_complete;
        common.b_rank_complete = special->b_rank_complete;
        common.a_rank_complete = special->a_rank_complete;
        common.s_rank_complete = special->s_rank_complete;
    } else if (const db::live::NormalLive *normal = context.db.live.get<db::live::NormalLive>(live_difficulty_id)) {
        common.normal = normal;
        common.live_difficulty_id = normal->live_difficulty_id;
        common.live_setting_id = normal->live_setting_id;
        common.capital_type = normal->capital_type;
        common.capital_value = normal->capital_value;
        common.c_rank_complete = normal->c_rank_complete;
        common.b_rank_complete = normal->b_rank_complete;
        common.a_rank_complete = normal->a_rank_complete;
        common.s_rank_complete = normal->s_rank_complete;
    } else
        return nullptr;
    return &cache.emplace(live_difficulty_id, common).first->second;
}

const db::live::LiveSetting *get_live_setting(Context &context, int64_t live_setting_id) {
    return db::get_decrypted_row<db::live::LiveSetting>(context.db.live, live_setting_id);
}

std::optional<int64_t> get_live_lp(Context &context, int64_t live_difficulty_id) {
    const CommonLive *live_info = get_live_info_table(context, live_difficulty_id);
    if (live_info == nullptr) return std::nullopt;
    return live_info->capital_value;
}

const db::live::LiveSetting *get_live_setting_from_difficulty_id(Context &context, int64_t live_difficulty_id) {
    const CommonLive *live_info = get_live_info_table(context, live_difficulty_id);
    if (live_info == nullptr) return nullptr;
    return get_live_setting(context, live_info->live_setting_id);
}

std::optional<LiveInfoWithNotes> get_live_info(Context &context, int64_t live_difficulty_id,
                                               const db::live::LiveSetting &live_setting) {
    auto beatmap_data = external::beatmap::get_beatmap_data(live_setting.notes_setting_asset, context);
    if (!beatmap_data) return std::nullopt;

    // TODO: Randomize
    LiveInfoWithNotes info;
    info.live_difficulty_id = live_difficulty_id;
    info.ac_flag = live_setting.ac_flag;
    info.swing_flag = live_setting.swing_flag;
    info.notes_list = notes_from_beatmap(*beatmap_data);
    return info;
}

LiveInfo get_live_info_without_notes(Context &, int64_t live_difficulty_id, const db::live::LiveSetting &live_setting) {
    LiveInfo info;
    info.live_difficulty_id = live_difficulty_id;
    info.ac_flag = live_setting.ac_flag;
    info.swing_flag = live_setting.swing_flag;
    return info;
}

std::vector<const db::live::LiveGoalReward *> get_goal_list_by_live_difficulty_id(Context &context,
                                                                                  int64_t live_difficulty_id) {
    // @context_cacheable("live_goal_reward")
    static std::map<int64_t, std::vector<const db::live::LiveGoalReward *>> cache;
    auto it = cache.find(live_difficulty_id);
    if (it != cache.end()) return it->second;
    auto rows = context.db.live.select<db::live::LiveGoalReward>("WHERE live_difficulty_id = ?", live_difficulty_id);
    cache.emplace(live_difficulty_id, rows);
    return rows;
}

std::map<const_::LIVE_GOAL_TYPE, std::vector<Range>> make_rank_range(const CommonLive &live_info,
                                                                     const db::live::LiveSetting &live_setting) {
    return {
        // Note: The ranges are in reverse order
        {const_::LIVE_GOAL_TYPE::SCORE,
         {{live_setting.s_rank_score, MAX_INT},
          {live_setting.a_rank_score, live_setting.s_rank_score},
          {live_setting.b_rank_score, live_setting.a_rank_score},
          {live_setting.c_rank_score, live_setting.b_rank_score}}},
        {const_::LIVE_GOAL_TYPE::COMBO,
         {{live_setting.s_rank_combo, MAX_INT},
          {live_setting.a_rank_combo, live_setting.s_rank_combo},
          {live_setting.b_rank_combo, live_setting.a_rank_combo},
          {live_setting.c_rank_combo, live_setting.b_rank_combo}}},
        {const_::LIVE_GOAL_TYPE::CLEAR,
         {{live_info.s_rank_complete, MAX_INT},
          {live_info.a_rank_complete, live_info.s_rank_complete},
          {live_info.b_rank_complete, live_info.a_rank_complete},
          {live_info.c_rank_complete, live_info.b_rank_complete}}},
    };
}

int64_t get_index_of_range(int64_t value, const std::vector<Range> &seq, int64_t start, int64_t default_) {
    int64_t i = start;
    for (const Range &r : seq) {
        if (r.contains(value)) return i;
        i++;
    }
    return default_;
}

std::tuple<int64_t, int64_t, int64_t> get_live_ranks(const CommonLive &live_info,
                                                     const db::live::LiveSetting &live_setting, int64_t score,
                                                     int64_t combo, int64_t clears) {
    auto rank_ranges = make_rank_range(live_info, live_setting);
    int64_t score_rank = get_index_of_range(score, rank_ranges[const_::LIVE_GOAL_TYPE::SCORE], 1, 5);
    int64_t combo_rank = get_index_of_range(combo, rank_ranges[const_::LIVE_GOAL_TYPE::COMBO], 1, 5);
    int64_t clear_rank = get_index_of_range(clears, rank_ranges[const_::LIVE_GOAL_TYPE::CLEAR], 1, 5);
    return {score_rank, combo_rank, clear_rank};
}

std::vector<int64_t> get_achieved_goal_id_list(Context &context, const db::main::LiveClear &clear_info) {
    const CommonLive *live_info = get_live_info_table(context, clear_info.live_difficulty_id);
    std::vector<int64_t> result;
    if (live_info != nullptr) {
        const db::live::LiveSetting *live_setting = get_live_setting(context, live_info->live_setting_id);
        if (live_setting != nullptr) {
            // Sort out the goal rewards
            auto goal_list = get_goal_list_by_live_difficulty_id(context, clear_info.live_difficulty_id);
            auto by_type = [&](const_::LIVE_GOAL_TYPE type) {
                std::vector<const db::live::LiveGoalReward *> out;
                for (const auto *g : goal_list)
                    if (g->live_goal_type == (int64_t)type) out.push_back(g);
                std::stable_sort(out.begin(), out.end(), [](const auto *a, const auto *b) { return a->rank < b->rank; });
                return out;
            };
            auto [score_rank, combo_rank, clear_rank] = get_live_ranks(
                *live_info, *live_setting, clear_info.hi_score, clear_info.hi_combo_cnt, clear_info.clear_cnt);
            for (const auto *g : by_type(const_::LIVE_GOAL_TYPE::SCORE))
                if (score_rank <= g->rank) result.push_back(g->live_goal_reward_id);
            for (const auto *g : by_type(const_::LIVE_GOAL_TYPE::COMBO))
                if (combo_rank <= g->rank) result.push_back(g->live_goal_reward_id);
            for (const auto *g : by_type(const_::LIVE_GOAL_TYPE::CLEAR))
                if (clear_rank <= g->rank) result.push_back(g->live_goal_reward_id);
        }
    }

    return result;
}

std::vector<model::Ptr<item_model::Item>> get_goal_rewards(Context &context, const std::vector<int64_t> &goal_ids) {
    std::vector<model::Ptr<item_model::Item>> out;
    if (goal_ids.empty()) return out;
    auto rows = context.db.live.select<db::live::LiveGoalReward>("WHERE live_goal_reward_id IN (" + in_list(goal_ids) +
                                                                 ")");
    for (const db::live::LiveGoalReward *k : rows) {
        auto it = model::make<item_model::Item>();
        it->add_type = (const_::ADD_TYPE)k->add_type;
        it->item_id = k->item_id;
        it->amount = k->amount;
        it->item_category_id = k->item_category_id.value_or(0);
        it->reward_box_flag = false;
        out.push_back(it);
    }
    return out;
}

std::vector<int64_t> get_live_setting_ids_from_track_id(Context &context, int64_t live_track_id) {
    // @context_cacheable("live_setting_ids_from_track")
    static std::map<int64_t, std::vector<int64_t>> cache;
    auto it = cache.find(live_track_id);
    if (it != cache.end()) return it->second;
    static const ScanIndex index = build_scan_index(
        context, std::string("SELECT live_track_id, live_setting_id FROM ") + db::live::LiveSetting::TABLE);
    std::vector<int64_t> ids = scan_lookup(index, {live_track_id});
    cache.emplace(live_track_id, ids);
    return ids;
}

bool has_normal_live_unlock(Context &context, db::main::User &user, int64_t live_track_id) {
    return context.db.main.first<db::main::NormalLiveUnlock>("WHERE user_id = ? AND live_track_id = ?", user.id,
                                                              live_track_id) != nullptr;
}

std::vector<int64_t> get_all_normal_live_unlock(Context &context, db::main::User &user) {
    db::Stmt &s = context.db.main.query("SELECT live_track_id FROM normal_live_unlock WHERE user_id = ?");
    s.bind_all(user.id);
    return read_ints(s);
}

db::main::LiveInProgress *get_live_in_progress(Context &context, db::main::User &user) {
    return context.db.main.first<db::main::LiveInProgress>("WHERE user_id = ?", user.id);
}

bool clean_live_in_progress(Context &context, db::main::User &user) {
    return context.db.main.execute("DELETE FROM live_in_progress WHERE user_id = ?", user.id) > 0;
}

void register_live_in_progress(Context &context, db::main::User &user, db::main::User &party_user, int64_t lp_factor,
                               int64_t unit_deck_id, const Bytes &deck_data_bytes) {
    db::main::LiveInProgress *live_in_progress = get_live_in_progress(context, user);
    if (live_in_progress == nullptr) {
        db::main::LiveInProgress wip;
        wip.user_id = user.id;
        wip.party_user_id = party_user.id;
        wip.lp_factor = lp_factor;
        wip.unit_deck_id = unit_deck_id;
        wip.deck_data = deck_data_bytes;
        context.db.main.add(wip);
    } else {
        live_in_progress->party_user_id = party_user.id;
        live_in_progress->lp_factor = lp_factor;
        live_in_progress->unit_deck_id = unit_deck_id;
        live_in_progress->deck_data = deck_data_bytes;
    }
    context.db.main.flush();
}

const db::live::LiveTrack *get_live_track_info(Context &context, int64_t live_track_id) {
    const db::live::LiveTrack *live_track = db::get_decrypted_row<db::live::LiveTrack>(context.db.live, live_track_id);
    if (live_track == nullptr)
        throw std::invalid_argument("track info of live_track_id " + std::to_string(live_track_id) + " not found");
    return live_track;
}

std::vector<std::pair<int64_t, std::vector<std::pair<int64_t, int64_t>>>>
get_special_live_rotation_time_modulo(Context &context) {
    auto rows = context.db.live.select<db::live::SpecialLiveRotation>("");
    std::vector<std::pair<int64_t, std::vector<std::pair<int64_t, int64_t>>>> rotation_group_time;

    for (const db::live::SpecialLiveRotation *row : rows) {
        auto it = std::find_if(rotation_group_time.begin(), rotation_group_time.end(),
                               [&](const auto &g) { return g.first == row->rotation_group_id; });
        if (it == rotation_group_time.end()) {
            rotation_group_time.emplace_back(row->rotation_group_id, std::vector<std::pair<int64_t, int64_t>>());
            it = rotation_group_time.end() - 1;
        }
        int64_t unix_timestamp = util::datetime_to_timestamp(row->base_date);
        it->second.emplace_back(row->live_difficulty_id, util::floordiv(unix_timestamp, 86400));
    }

    return rotation_group_time;
}

std::vector<std::pair<int64_t, int64_t>> get_special_live_rotation_difficulty_id(Context &context) {
    auto rotation_group_time = get_special_live_rotation_time_modulo(context);
    std::vector<std::pair<int64_t, int64_t>> result;
    int64_t current_day = util::get_days_since_unix(util::time());

    for (const auto &[group_id, live_list] : rotation_group_time) {
        int64_t n = (int64_t)live_list.size();
        int64_t current_day_modulo = util::pymod(current_day, n);

        for (const auto &[live_difficulty_id, day_time] : live_list) {
            if (util::pymod(day_time, n) == current_day_modulo) {
                auto it = std::find_if(result.begin(), result.end(), [&](const auto &p) { return p.first == group_id; });
                if (it == result.end()) result.emplace_back(group_id, live_difficulty_id);
                else it->second = live_difficulty_id;
                break;
            }
        }
    }

    return result;
}

std::vector<LiveStatus> get_special_live_status(Context &context, db::main::User &user) {
    std::vector<LiveStatus> result;
    auto today_b_side_rotation = get_special_live_rotation_difficulty_id(context);

    for (const auto &entry : today_b_side_rotation) {
        int64_t live_difficulty_id = entry.second;
        db::main::LiveClear *live_clear = get_live_clear_data(context, user, live_difficulty_id);
        result.push_back(live_status_from_live_clear(context, live_difficulty_id, live_clear));
    }

    return result;
}

std::vector<int64_t> get_training_live_difficulty_id_from_live_track_id(Context &context, int64_t live_track_id) {
    // @context_cacheable("live_training_from_track_id")
    static std::map<int64_t, std::vector<int64_t>> cache;
    auto cached = cache.find(live_track_id);
    if (cached != cache.end()) return cached->second;

    static const ScanIndex training_settings = build_scan_index(
        context, std::string("SELECT live_track_id, live_setting_id FROM ") + db::live::LiveSetting::TABLE +
                     " WHERE difficulty > 5 AND ac_flag = 0");
    static const ScanIndex special_by_setting = build_scan_index(
        context, std::string("SELECT live_setting_id, live_difficulty_id FROM ") + db::live::SpecialLive::TABLE +
                     " WHERE exclude_clear_count_flag = 0");
    static const std::set<int64_t> rotation = [&] {
        db::Stmt &s = context.db.live.query(std::string("SELECT live_difficulty_id FROM ") +
                                            db::live::SpecialLiveRotation::TABLE);
        std::vector<int64_t> ids = read_ints(s);
        return std::set<int64_t>(ids.begin(), ids.end());
    }();

    std::vector<int64_t> live_setting_ids = scan_lookup(training_settings, {live_track_id});
    std::vector<int64_t> found = scan_lookup(special_by_setting, live_setting_ids);
    // Make sure it's not in special_live_rotation_m
    std::set<int64_t> live_difficulty_ids(found.begin(), found.end());

    std::vector<int64_t> result;
    for (int64_t id : live_difficulty_ids)
        if (!rotation.count(id)) result.push_back(id);  // sorted
    cache.emplace(live_track_id, result);
    return result;
}

std::vector<LiveStatus> get_training_live_clear_status_of_track(Context &context, db::main::User &user,
                                                                int64_t live_track_id) {
    auto live_difficulty_ids = get_training_live_difficulty_id_from_live_track_id(context, live_track_id);
    std::vector<LiveStatus> live_status_result;

    for (int64_t live_difficulty_id : live_difficulty_ids) {
        db::main::LiveClear *live_clear = get_live_clear_data(context, user, live_difficulty_id);
        live_status_result.push_back(live_status_from_live_clear(context, live_difficulty_id, live_clear));
    }

    return live_status_result;
}

std::vector<LiveStatus> get_training_live_status(Context &context, db::main::User &user) {
    std::vector<LiveStatus> live_status_result;

    // Get normal live unlock
    for (int64_t live_track_id : get_all_normal_live_unlock(context, user)) {
        auto status = get_training_live_clear_status_of_track(context, user, live_track_id);
        live_status_result.insert(live_status_result.end(), status.begin(), status.end());
    }

    return live_status_result;
}

Json notes_list_dump(const std::vector<LiveNote> &notes_list) { return model::dump(notes_list); }

std::string compact_json_dump(const Json &value) {
    std::string out;
    compact_dump(out, value);
    return out;
}

std::string notes_list_dump_json(const std::vector<LiveNote> &notes_list) {
    return compact_json_dump(notes_list_dump(notes_list));
}

std::vector<LiveNote> notes_list_validate_json(std::string_view json) {
    return model::parse<std::vector<LiveNote>>(Json::parse(json));
}

std::tuple<int64_t, Bytes> record_notes_list(Context &context, const std::vector<LiveNote> &notes_list) {
    std::string notes_list_json = notes_list_dump_json(notes_list);
    Bytes sha256 = crypto::sha256(notes_list_json);
    int64_t crc32_value = (int64_t)::crc32(0, (const Bytef *)notes_list_json.data(), (uInt)notes_list_json.size());

    if (config::store_backup_of_notes_list()) {
        db::Stmt &s = context.db.main.query("SELECT id FROM notes_list_backup WHERE crc32 = ? AND sha256 = ? LIMIT 1");
        s.bind(1, crc32_value);
        s.bind_blob(2, sha256);
        bool found = s.step();
        s.reset();

        if (!found) {
            db::main::NotesListBackup backup;
            backup.crc32 = crc32_value;
            backup.sha256 = sha256;
            backup.notes_list = gzip_compress(notes_list_json);
            context.db.main.add(backup);
            context.db.main.flush();
        }
    }

    return {crc32_value, sha256};
}

void record_precise_score(Context &context, db::main::User &user, int64_t live_difficulty_id, bool use_skill,
                          const std::vector<LiveNote> &notes_list, const Json &precise_log_data) {
    db::main::LiveReplay *replay = context.db.main.first<db::main::LiveReplay>(
        "WHERE user_id = ? AND live_difficulty_id = ? AND use_skill = ?", user.id, live_difficulty_id, use_skill);

    if (replay == nullptr) {
        db::main::LiveReplay row;
        row.user_id = user.id;
        row.live_difficulty_id = live_difficulty_id;
        row.use_skill = use_skill;
        row.timestamp = 0;
        row.notes_crc32 = 0;
        row.notes_sha256 = "";
        row.precise_log = "";
        replay = context.db.main.add(row);
    }

    replay->timestamp = util::time();
    auto [crc, sha] = record_notes_list(context, notes_list);
    replay->notes_crc32 = crc;
    replay->notes_sha256 = sha;
    replay->precise_log = gzip_compress(precise_log_data.dump());
    context.db.main.flush();
}

std::optional<std::tuple<Json, std::vector<LiveNote>, int64_t>>
pull_precise_score_with_beatmap(Context &context, db::main::User &user, int64_t live_difficulty_id, bool use_skill) {
    db::main::LiveReplay *replay = context.db.main.first<db::main::LiveReplay>(
        "WHERE user_id = ? AND live_difficulty_id = ? AND use_skill = ?", user.id, live_difficulty_id, use_skill);
    if (replay == nullptr) return std::nullopt;

    // Get notes cache
    std::optional<std::string> notes_list_bytes;
    {
        db::Stmt &s = context.db.main.query("SELECT notes_list FROM notes_list_backup WHERE crc32 = ? AND sha256 = ?");
        s.bind(1, replay->notes_crc32);
        s.bind_blob(2, replay->notes_sha256);
        if (s.step()) notes_list_bytes = s.get_blob(0);
        s.reset();
    }
    std::optional<std::vector<LiveNote>> notes_list;
    if (notes_list_bytes) notes_list = notes_list_validate_json(gzip_decompress(*notes_list_bytes));

    if (!notes_list) {
        // Try look at current beatmap
        const db::live::LiveSetting *live_setting = get_live_setting_from_difficulty_id(context, live_difficulty_id);
        if (live_setting == nullptr) return std::nullopt;

        auto beatmap_data = external::beatmap::get_beatmap_data(live_setting->notes_setting_asset, context);
        if (!beatmap_data) return std::nullopt;

        std::vector<LiveNote> notes_list_unprocessed = notes_from_beatmap(*beatmap_data);
        Bytes sha256_of_notes = crypto::sha256(notes_list_dump_json(notes_list_unprocessed));

        if (replay->notes_sha256 == sha256_of_notes) notes_list = std::move(notes_list_unprocessed);
    }

    if (!notes_list) return std::nullopt;

    return std::make_tuple(Json::parse(gzip_decompress(replay->precise_log)), std::move(*notes_list),
                           replay->timestamp);
}

std::map<int64_t, int64_t> get_cleared_live_count(Context &context, db::main::User &) {
    // NPPS4 does not filter by user here.
    db::Stmt &s = context.db.main.query(
        "SELECT difficulty, count(live_difficulty_id) FROM live_clear GROUP BY difficulty");
    std::map<int64_t, int64_t> result;
    while (s.step()) result[s.get_int(0)] = s.get_int(1);
    s.reset();
    return result;
}

std::map<int64_t, int64_t> get_enh_live_difficulty_ids(Context &context, int64_t live_difficulty_id) {
    // @context_cacheable("adjacent_live_difficulty_id")
    auto &cache = context.memo<std::map<int64_t, std::map<int64_t, int64_t>>>("adjacent_live_difficulty_id");
    auto cached = cache.find(live_difficulty_id);
    if (cached != cache.end()) return cached->second;

    std::map<int64_t, int64_t> output;

    const CommonLive *live_info_base = get_live_info_table(context, live_difficulty_id);
    if (live_info_base == nullptr)
        throw std::invalid_argument("invalid live_difficulty_id " + std::to_string(live_difficulty_id));

    const db::live::LiveSetting *live_setting = get_live_setting(context, live_info_base->live_setting_id);
    if (live_setting == nullptr)
        throw std::invalid_argument("invalid live_setting_id " + std::to_string(live_info_base->live_setting_id));

    auto live_settings = context.db.live.select<db::live::LiveSetting>("WHERE live_track_id = ? AND difficulty <= 3",
                                                                       live_setting->live_track_id);
    std::map<int64_t, const db::live::LiveSetting *> live_setting_map;
    std::vector<int64_t> setting_ids;
    for (const auto *l : live_settings) {
        live_setting_map[l->live_setting_id] = l;
        setting_ids.push_back(l->live_setting_id);
    }

    std::vector<std::pair<int64_t, int64_t>> infos;  // (live_setting_id, live_difficulty_id)
    if (!setting_ids.empty()) {
        std::string tail = "WHERE live_setting_id IN (" + in_list(setting_ids) + ")";
        if (live_info_base->special)
            for (const auto *l : context.db.live.select<db::live::SpecialLive>(tail))
                infos.emplace_back(l->live_setting_id, l->live_difficulty_id);
        else
            for (const auto *l : context.db.live.select<db::live::NormalLive>(tail))
                infos.emplace_back(l->live_setting_id, l->live_difficulty_id);
    }

    for (const auto &[setting_id, difficulty_id] : infos) {
        output[live_setting_map.at(setting_id)->difficulty] = difficulty_id;
        // Also set cache for the retrieved live difficulty ids
        cache[difficulty_id] = output;
    }
    cache[live_difficulty_id] = output;
    // The cached dicts are one shared object in NPPS4; bring earlier entries up to date.
    for (const auto &[setting_id, difficulty_id] : infos) cache[difficulty_id] = output;

    return output;
}

}  // namespace npps::system::live
