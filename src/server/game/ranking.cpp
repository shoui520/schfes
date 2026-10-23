// npps4/game/ranking.py
#include "app/idol.hpp"
#include "system/ranking.hpp"
#include "system/reward.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

namespace npps::game::ranking {

namespace sys = ::npps::system;

// Shared by ranking/live and ranking/player (NPPS4 duplicates this code in both).
// Returns null when the player has no usable center unit info (a `continue` in Python).
static std::optional<RankingData> make_ranking_data(Context &context, int64_t rank, int64_t score,
                                                    db::main::User &target_user) {
    // Get unit center info
    int64_t unit_center = sys::unit::get_unit_center(context, target_user);

    db::main::Unit *unit_data = sys::unit::get_unit(context, unit_center);
    sys::unit::validate_unit(target_user, unit_data);
    sys::unit::get_unit_info(context, unit_data->unit_id);

    std::vector<int64_t> removable_skills = sys::unit::get_unit_removable_skills(context, *unit_data);
    auto [unit_full_data, unit_stats] = sys::unit::get_unit_data_full_info(context, *unit_data);

    auto center = model::make<sys::common::CenterUnitInfo>();
    center->unit_id = unit_data->unit_id;
    center->level = unit_full_data.level;
    center->rank = unit_data->rank;
    center->love = unit_data->love;
    center->display_rank = unit_data->display_rank;
    center->unit_skill_exp = unit_data->skill_exp;
    center->unit_removable_skill_capacity = unit_data->unit_removable_skill_capacity;
    center->smile = unit_stats.smile;
    center->cute = unit_stats.pure;
    center->cool = unit_stats.cool;
    center->is_love_max = unit_full_data.is_love_max;
    center->is_level_max = unit_full_data.is_level_max;
    center->is_rank_max = unit_full_data.is_rank_max;
    center->removable_skill_ids = removable_skills;

    auto user_data = model::make<models::UserData>();
    user_data->user_id = target_user.id;
    user_data->name = target_user.name;
    user_data->level = target_user.level;

    return RankingData::make(rank, score, user_data, center, target_user.active_award);
}

static RankingResponse ranking_live(Context &context, const RankingLiveRequest &request) {
    db::main::User *current_user = sys::user::get_current(context);
    auto [total_cnt, player_scores] =
        sys::ranking::get_live_ranking(context, request.live_difficulty_id, request.page);

    std::vector<RankingData> rank_player_scores;
    int64_t i = 1;
    for (const auto &[user_id, score] : player_scores) {
        int64_t rank = i++;
        // TODO: Deduplicate with partyInfo code
        db::main::User *target_user = sys::user::get(context, user_id);
        if (target_user == nullptr) continue;

        rank_player_scores.push_back(*make_ranking_data(context, rank, score, *target_user));
    }

    auto response = RankingResponse::make(std::nullopt, std::move(rank_player_scores), total_cnt,
                                          sys::reward::count_presentbox(context, *current_user));
    response.page = request.page;
    return response;
}
NPPS_ENDPOINT("ranking", "live", User, ranking_live);

static RankingResponse ranking_player(Context &context, const RankingPlayerRequest &request) {
    db::main::User *current_user = sys::user::get_current(context);
    int64_t page = request.page;

    std::vector<db::main::PlayerRanking *> rankings;
    if (request.id > 0) {
        auto result = sys::ranking::get_daily_ranking_by_user(context, request.id, request.daily_index == 2);

        if (!result) throw error::by_code(error::ERROR_CODE_USER_NOT_EXIST);

        rankings = std::move(std::get<0>(*result));
        page = std::get<1>(*result);
    } else
        rankings = sys::ranking::get_daily_ranking(context, page, request.daily_index == 2);

    int64_t total_count = sys::ranking::count_daily_ranking(context, request.daily_index == 2);

    std::vector<RankingData> items;
    std::optional<int64_t> current_rank;

    int64_t i = request.page * 20 + 1;
    for (const db::main::PlayerRanking *rank : rankings) {
        int64_t rank_number = i++;
        db::main::User *target_user = sys::user::get(context, rank->user_id);
        if (target_user == nullptr) continue;

        if (rank->user_id == current_user->id) current_rank = rank_number;

        items.push_back(*make_ranking_data(context, rank_number, rank->score, *target_user));
    }

    if (items.empty()) throw error::by_code(error::ERROR_CODE_OUT_OF_RANG);

    auto response = RankingResponse::make(current_rank, std::move(items), total_count,
                                          sys::reward::count_presentbox(context, *current_user));
    response.page = page;
    return response;
}
NPPS_ENDPOINT("ranking", "player", User, ranking_player);

}  // namespace npps::game::ranking
