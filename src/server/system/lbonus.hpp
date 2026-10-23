#pragma once
// npps4/system/lbonus.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <set>
#include <tuple>
#include <vector>

namespace npps::system::lbonus {

bool has_login_bonus(Context &context, db::main::User &user, int64_t year, int64_t month, int64_t day);
std::set<int64_t> days_login_bonus(Context &context, db::main::User &user, int64_t year, int64_t month);
void mark_login_bonus(Context &context, db::main::User &user, int64_t year, int64_t month, int64_t day);
std::vector<LoginBonusCalendar> get_calendar(Context &context, int64_t year, int64_t month);
int64_t get_login_count(Context &context, db::main::User &user);
// (day, month, year)
std::vector<std::tuple<int64_t, int64_t, int64_t>> all_login_bonus(Context &context, db::main::User &user);

}  // namespace npps::system::lbonus
