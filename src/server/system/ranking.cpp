// npps4/system/ranking.py
#include "system/ranking.hpp"

namespace npps::system::ranking {

std::tuple<int64_t, std::vector<std::pair<int64_t, int64_t>>> get_live_ranking(Context &context,
                                                                               int64_t live_difficulty_id,
                                                                               int64_t page) {
    int64_t total_cnt = context.db.main.count<db::main::LiveClear>("WHERE live_difficulty_id = ? AND clear_cnt > 0",
                                                                    live_difficulty_id);
    std::vector<std::pair<int64_t, int64_t>> player_scores;

    if (total_cnt > 0) {
        auto rows = context.db.main.select<db::main::LiveClear>(
            "WHERE live_difficulty_id = ? AND clear_cnt > 0 ORDER BY hi_score DESC LIMIT ? OFFSET ?",
            live_difficulty_id, QUERY_PER_PAGE, page * QUERY_PER_PAGE);
        for (const db::main::LiveClear *row : rows) player_scores.emplace_back(row->user_id, row->hi_score);
    }

    return {total_cnt, player_scores};
}

static bool currently_cleaning = false;

void try_cleanup_daily_rankings(int64_t current_day_index) {
    if (!currently_cleaning) {
        currently_cleaning = true;
        // NPPS4 sleeps 5 seconds here (asyncio.sleep); the single-threaded server must not block.
        try {
            Context context;
            int64_t two_days_ago = current_day_index - 2;
            context.db.main.execute("DELETE FROM player_ranking WHERE day <= ?", two_days_ago);
            context.commit();
        } catch (...) {
            currently_cleaning = false;
            throw;
        }
        currently_cleaning = false;
    }
}

void increment_daily_score(Context &context, db::main::User &user, int64_t inc_score) {
    int64_t current_day_index = util::get_days_since_unix(util::time());

    if (context.support_background_task())
        context.add_task([current_day_index] { try_cleanup_daily_rankings(current_day_index); });

    db::main::PlayerRanking *player_score =
        context.db.main.first<db::main::PlayerRanking>("WHERE user_id = ? AND day = ?", user.id, current_day_index);

    if (player_score == nullptr) {
        db::main::PlayerRanking row;
        row.user_id = user.id;
        row.day = current_day_index;
        row.score = 0;
        player_score = context.db.main.add(row);
    }

    player_score->score = player_score->score + inc_score;
    context.db.main.flush();
}

int64_t count_daily_ranking(Context &context, bool yesterday) {
    int64_t day_index = util::get_days_since_unix(util::time()) - (int64_t)yesterday;
    return context.db.main.count<db::main::PlayerRanking>("WHERE day = ?", day_index);
}

std::vector<db::main::PlayerRanking *> get_daily_ranking(Context &context, int64_t page, bool yesterday) {
    int64_t day_index = util::get_days_since_unix(util::time()) - (int64_t)yesterday;
    return context.db.main.select<db::main::PlayerRanking>(
        "WHERE day = ? ORDER BY score DESC, user_id LIMIT 20 OFFSET ?", day_index, page * 20);
}

std::optional<std::tuple<std::vector<db::main::PlayerRanking *>, int64_t>>
get_daily_ranking_by_user(Context &context, int64_t user_id, bool yesterday) {
    int64_t day_index = util::get_days_since_unix(util::time()) - (int64_t)yesterday;

    db::main::PlayerRanking *target =
        context.db.main.first<db::main::PlayerRanking>("WHERE user_id = ? AND day = ?", user_id, day_index);
    if (target == nullptr) return std::nullopt;

    int64_t players_before = context.db.main.count<db::main::PlayerRanking>(
        "WHERE day = ? AND (score > ? OR (score = ? AND id < ?))", day_index, target->score, target->score, target->id);

    int64_t page_index = players_before / 20;
    int64_t page_offset = page_index * 20;

    auto rows = context.db.main.select<db::main::PlayerRanking>(
        "WHERE day = ? ORDER BY score DESC, id ASC LIMIT 20 OFFSET ?", day_index, page_offset);
    return std::make_tuple(std::move(rows), page_index);
}

}  // namespace npps::system::ranking
