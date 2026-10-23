#pragma once
// external/login_bonus.py: the login bonus calendar (config.get_login_bonus_protocol()).
#include "app/context.hpp"

#include <optional>
#include <string>
#include <tuple>

namespace npps::external::login_bonus {

// Special day image asset (Japanese, optional English).
using SpecialDay = std::tuple<std::string, std::optional<std::string>>;

// Returns (add_type, item_id, amount, special_day).
std::tuple<int64_t, int64_t, int64_t, std::optional<SpecialDay>> get_rewards(int64_t day, int64_t month, int64_t year,
                                                                             Context &context);

}  // namespace npps::external::login_bonus
