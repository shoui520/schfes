// npps4/system/album.py
#include "system/album.hpp"

#include <algorithm>

namespace npps::system::album {

void update(Context &context, db::main::User &user, int64_t unit_id, bool rank_max, bool love_max,
            bool rank_level_max, IntOrIncrement highest_love, IntOrIncrement favorite_point, bool sign_flag,
            bool flush) {
    db::main::Album *album =
        context.db.main.first<db::main::Album>("WHERE user_id = ? AND unit_id = ?", user.id, unit_id);

    if (!album) {
        db::main::Album row;
        row.user_id = user.id;
        row.unit_id = unit_id;
        album = context.db.main.add(row);
    }

    album->rank_max_flag = rank_max || album->rank_max_flag;
    album->love_max_flag = love_max || album->love_max_flag;
    album->rank_level_max_flag = rank_level_max || album->rank_level_max_flag;
    album->sign_flag = sign_flag || album->sign_flag;

    if (auto *inc = std::get_if<Increment>(&highest_love)) album->highest_love_per_unit += inc->value;
    else album->highest_love_per_unit = std::max(album->highest_love_per_unit, std::get<int64_t>(highest_love));
    if (auto *inc = std::get_if<Increment>(&favorite_point)) album->favorite_point += inc->value;
    else album->favorite_point = std::max(album->favorite_point, std::get<int64_t>(favorite_point));

    if (flush) context.db.main.flush();
}

std::vector<db::main::Album *> all(Context &context, const db::main::User &user) {
    return context.db.main.select<db::main::Album>("WHERE user_id = ?", user.id);
}

std::vector<db::main::Album *> all_ranking(Context &context, const db::main::User &user) {
    return context.db.main.select<db::main::Album>("WHERE user_id = ? ORDER BY favorite_point DESC, id DESC LIMIT 50",
                                                   user.id);
}

std::map<int64_t, std::vector<game::album::AlbumInfo>> all_series(Context &context) {
    std::map<int64_t, std::vector<game::album::AlbumInfo>> out;
    for (const auto *series : context.db.unit.select<db::unit::AlbumSeries>("")) out[series->album_series_id];
    return out;
}

int64_t count_album_with(Context &context, const db::main::User &user, const std::string &criteria) {
    std::string tail = "WHERE user_id = ?";
    if (!criteria.empty()) tail += " AND (" + criteria + ")";
    return context.db.main.count<db::main::Album>(tail, user.id);
}

bool has_ever_got_unit(Context &context, const db::main::User &user, int64_t unit_id) {
    return context.db.main.first<db::main::Album>("WHERE user_id = ? AND unit_id = ?", user.id, unit_id) != nullptr;
}

}  // namespace npps::system::album
