#pragma once
// npps4/system/ranking.py
#include "app/context.hpp"

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace npps::system::ranking {

constexpr int64_t QUERY_PER_PAGE = 20;

// (total_cnt, [(user_id, hi_score)])
std::tuple<int64_t, std::vector<std::pair<int64_t, int64_t>>> get_live_ranking(Context &context,
                                                                               int64_t live_difficulty_id,
                                                                               int64_t page);
// Background task: opens its own context.
void try_cleanup_daily_rankings(int64_t current_day_index);
void increment_daily_score(Context &context, db::main::User &user, int64_t inc_score);
int64_t count_daily_ranking(Context &context, bool yesterday);
// Pagination is hardcoded to 20 entries per page.
std::vector<db::main::PlayerRanking *> get_daily_ranking(Context &context, int64_t page, bool yesterday);
// (adjacent players, current page), or nullopt when the user has no ranking that day.
std::optional<std::tuple<std::vector<db::main::PlayerRanking *>, int64_t>>
get_daily_ranking_by_user(Context &context, int64_t user_id, bool yesterday);

}  // namespace npps::system::ranking
