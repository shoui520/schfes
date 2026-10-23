// npps4/system/museum.py
#include "system/museum.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace npps::system::museum {

bool unlock(Context &context, db::main::User &user, int64_t museum_contents_id) {
    if (context.db.museum.get<db::museum::MuseumContents>(museum_contents_id) == nullptr)
        throw std::invalid_argument("invalid museum contents id");

    if (has(context, user, museum_contents_id)) return false;

    db::main::MuseumUnlock museum_unlock;
    museum_unlock.user_id = user.id;
    museum_unlock.museum_contents_id = museum_contents_id;
    context.db.main.add(museum_unlock);
    context.db.main.flush();
    return true;
}

bool has(Context &context, db::main::User &user, int64_t museum_contents_id) {
    return context.db.main.first<db::main::MuseumUnlock>("WHERE \"user_id\" = ? AND \"museum_contents_id\" = ?",
                                                         user.id, museum_contents_id) != nullptr;
}

MuseumInfoData get_museum_info_data(Context &context, db::main::User &user) {
    std::vector<int64_t> contents_id_list;
    if (TEST_MUSEUM_UNLOCK_ALL) {
        for (const auto *mu : context.db.museum.select<db::museum::MuseumContents>(""))
            contents_id_list.push_back(mu->museum_contents_id);
    } else {
        for (const auto *mu : context.db.main.select<db::main::MuseumUnlock>("WHERE \"user_id\" = ?", user.id))
            contents_id_list.push_back(mu->museum_contents_id);
    }

    MuseumParameterData parameter;
    // Chunked to stay below SQLite's bound parameter limit; the sums don't depend on row order.
    constexpr size_t CHUNK = 500;
    for (size_t start = 0; start < contents_id_list.size(); start += CHUNK) {
        size_t end = std::min(start + CHUNK, contents_id_list.size());
        std::string sql = std::string("SELECT ") + db::museum::MuseumContents::COLUMNS + " FROM \"" +
                          db::museum::MuseumContents::TABLE + "\" WHERE \"museum_contents_id\" IN (";
        for (size_t i = start; i < end; i++) sql += i > start ? ", ?" : "?";
        sql += ")";
        db::Stmt &stmt = context.db.museum.query(sql);
        int param = 1;
        for (size_t i = start; i < end; i++) stmt.bind(param++, contents_id_list[i]);
        db::museum::MuseumContents row;
        while (stmt.step()) {
            row.read(stmt, 0);
            parameter.smile += row.smile_buff;
            parameter.pure += row.pure_buff;
            parameter.cool += row.cool_buff;
        }
        stmt.reset();
    }

    MuseumInfoData result;
    result.parameter = parameter;
    result.contents_id_list = std::move(contents_id_list);
    return result;
}

}  // namespace npps::system::museum
