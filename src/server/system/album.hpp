#pragma once
// npps4/system/album.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace npps::system::album {

struct Increment {
    int64_t value = 0;
};

// int | Increment
using IntOrIncrement = std::variant<int64_t, Increment>;

void update(Context &context, db::main::User &user, int64_t unit_id, bool rank_max = false, bool love_max = false,
            bool rank_level_max = false, IntOrIncrement highest_love = int64_t(0),
            IntOrIncrement favorite_point = int64_t(0), bool sign_flag = false, bool flush = true);

std::vector<db::main::Album *> all(Context &context, const db::main::User &user);

// Top 50 by favorite_point desc, id desc.
std::vector<db::main::Album *> all_ranking(Context &context, const db::main::User &user);

// {album_series_id: []} for every unit.AlbumSeries row.
std::map<int64_t, std::vector<game::album::AlbumInfo>> all_series(Context &context);

// *criteria: an SQL condition on the album table ANDed to "user_id = ?", e.g. "rank_max_flag = 1";
// empty for none.
int64_t count_album_with(Context &context, const db::main::User &user, const std::string &criteria = "");

bool has_ever_got_unit(Context &context, const db::main::User &user, int64_t unit_id);

}  // namespace npps::system::album
