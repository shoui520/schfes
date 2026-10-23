// npps4/game/album.py
#include "app/idol.hpp"
#include "system/album.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <algorithm>

namespace npps::game::album {

static AlbumInfo album_to_response(const db::main::Album &data) {
    return AlbumInfo::make(data.unit_id, data.rank_max_flag, data.love_max_flag, data.rank_level_max_flag,
                           data.rank_max_flag && data.love_max_flag && data.rank_level_max_flag,
                           data.highest_love_per_unit,
                           data.highest_love_per_unit,  // TODO: Rectify this.
                           data.favorite_point, data.sign_flag);
}

static std::vector<AlbumInfo> album_albumall(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<AlbumInfo> result;
    for (const db::main::Album *a : system::album::all(context, *current_user)) result.push_back(album_to_response(*a));
    return result;
}
NPPS_ENDPOINT("album", "albumAll", User, album_albumall);

static std::vector<AlbumSeriesInfo> album_seriesall(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<db::main::Album *> all_album = system::album::all(context, *current_user);
    std::map<int64_t, std::vector<AlbumInfo>> series = system::album::all_series(context);

    for (const db::main::Album *a : all_album) {
        const db::unit::Unit *unit_info = system::unit::get_unit_info(context, a->unit_id);
        if (unit_info && unit_info->album_series_id) series[*unit_info->album_series_id].push_back(album_to_response(*a));
    }

    // std::map is already ordered by series_id.
    std::vector<AlbumSeriesInfo> result;
    for (auto &[k, v] : series) {
        std::stable_sort(v.begin(), v.end(), [](const AlbumInfo &a, const AlbumInfo &b) { return a.unit_id < b.unit_id; });
        result.push_back(AlbumSeriesInfo::make(k, std::move(v)));
    }
    return result;
}
NPPS_ENDPOINT("album", "seriesAll", User, album_seriesall);

}  // namespace npps::game::album
