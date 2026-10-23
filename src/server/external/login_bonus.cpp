// external/login_bonus.py
#include "external/login_bonus.hpp"

namespace npps::external::login_bonus {

// Days from 1970-01-01 to a proleptic Gregorian date.
static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

std::tuple<int64_t, int64_t, int64_t, std::optional<SpecialDay>> get_rewards(int64_t day, int64_t month, int64_t year,
                                                                             Context &context) {
    (void)context;
    int64_t delta = days_from_civil(year, month, day);
    switch (util::pymod(delta, 3)) {
    case 0:
        // 20000 G
        return {3000, 3, 20000, std::nullopt};
    case 1:
        // 2500 Friend Points
        return {3002, 2, 2500, std::nullopt};
    default:
        // 1 Loveca
        return {3001, 4, 1, std::nullopt};
    }
}

}  // namespace npps::external::login_bonus
