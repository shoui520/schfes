// npps4/system/lbonus.py
#include "system/lbonus.hpp"

#include "external/login_bonus.hpp"
#include "system/advanced.hpp"

namespace npps::system::lbonus {

bool has_login_bonus(Context &context, db::main::User &user, int64_t year, int64_t month, int64_t day) {
    return context.db.main.first<db::main::LoginBonus>("WHERE user_id = ? AND year = ? AND month = ? AND day = ?",
                                                       user.id, year, month, day) != nullptr;
}

std::set<int64_t> days_login_bonus(Context &context, db::main::User &user, int64_t year, int64_t month) {
    std::set<int64_t> result;
    for (const auto *lb :
         context.db.main.select<db::main::LoginBonus>("WHERE user_id = ? AND year = ? AND month = ?", user.id, year, month))
        result.insert(lb->day);
    return result;
}

void mark_login_bonus(Context &context, db::main::User &user, int64_t year, int64_t month, int64_t day) {
    db::main::LoginBonus lbonus_data;
    lbonus_data.user_id = user.id;
    lbonus_data.year = year;
    lbonus_data.month = month;
    lbonus_data.day = day;
    context.db.main.add(lbonus_data);
    context.db.main.flush();
}

// calendar.monthrange(year, month) -> (weekday of the 1st with Monday = 0, number of days)
static std::pair<int64_t, int64_t> monthrange(int64_t year, int64_t month) {
    auto days_from_civil = [](int64_t y, int64_t m, int64_t d) {
        y -= m <= 2;
        int64_t era = (y >= 0 ? y : y - 399) / 400;
        int64_t yoe = y - era * 400;
        int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    };
    static const int64_t mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    int64_t days = mdays[month - 1] + (month == 2 && leap ? 1 : 0);
    return {util::pymod(days_from_civil(year, month, 1) + 3, 7), days};
}

std::vector<LoginBonusCalendar> get_calendar(Context &context, int64_t year, int64_t month) {
    auto [weekday, days] = monthrange(year, month);
    std::vector<LoginBonusCalendar> result;

    for (int64_t day = 1; day <= days; day++) {
        auto [add_type, item_id, amount, special] = external::login_bonus::get_rewards(day, month, year, context);
        item_model::BaseItem item_base;
        item_base.add_type = (const_::ADD_TYPE)add_type;
        item_base.item_id = item_id;
        item_base.amount = amount;
        int64_t dotw = (weekday + day) % 7;
        LoginBonusCalendar lbonus_calendar;
        lbonus_calendar.day = day;
        lbonus_calendar.day_of_the_week = dotw;
        lbonus_calendar.received = false;
        lbonus_calendar.item = advanced::deserialize_item_data(context, item_base);

        if (special) {
            const auto &second = std::get<1>(*special);
            const std::string &chosen = context.lang == Language::en && second ? *second : std::get<0>(*special);
            // special[1 if en else 0] or special[0]
            const std::string &special_asset = chosen.empty() ? std::get<0>(*special) : chosen;
            lbonus_calendar.special_day = true;
            lbonus_calendar.special_image_asset = special_asset;
        }

        result.push_back(std::move(lbonus_calendar));
    }
    return result;
}

int64_t get_login_count(Context &context, db::main::User &user) {
    return context.db.main.count<db::main::LoginBonus>("WHERE user_id = ?", user.id);
}

std::vector<std::tuple<int64_t, int64_t, int64_t>> all_login_bonus(Context &context, db::main::User &user) {
    std::vector<std::tuple<int64_t, int64_t, int64_t>> out;
    for (const auto *lb : context.db.main.select<db::main::LoginBonus>("WHERE user_id = ?", user.id))
        out.emplace_back(lb->day, lb->month, lb->year);
    return out;
}

}  // namespace npps::system::lbonus
