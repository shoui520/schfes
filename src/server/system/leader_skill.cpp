// npps4/leader_skill.py
#include "system/leader_skill.hpp"

#include <cmath>

namespace npps::leader_skill {

static int64_t ceil_by(int64_t value, double by) { return (int64_t)std::ceil((double)value * by); }

Bonus inc_smile(int64_t smile, int64_t, int64_t, double by) { return {ceil_by(smile, by), 0, 0}; }
Bonus inc_pure(int64_t, int64_t pure, int64_t, double by) { return {0, ceil_by(pure, by), 0}; }
Bonus inc_cool(int64_t, int64_t, int64_t cool, double by) { return {0, 0, ceil_by(cool, by)}; }
Bonus inc_smile_by_pure(int64_t, int64_t pure, int64_t, double by) { return {ceil_by(pure, by), 0, 0}; }
Bonus inc_smile_by_cool(int64_t, int64_t, int64_t cool, double by) { return {ceil_by(cool, by), 0, 0}; }
Bonus inc_pure_by_smile(int64_t smile, int64_t, int64_t, double by) { return {0, ceil_by(smile, by), 0}; }
Bonus inc_pure_by_cool(int64_t, int64_t, int64_t cool, double by) { return {0, ceil_by(cool, by), 0}; }
Bonus inc_cool_by_smile(int64_t smile, int64_t, int64_t, double by) { return {0, 0, ceil_by(smile, by)}; }
Bonus inc_cool_by_pure(int64_t, int64_t pure, int64_t, double by) { return {0, 0, ceil_by(pure, by)}; }

static Bonus return_zero(int64_t, int64_t, int64_t, double) { return {0, 0, 0}; }

const std::map<int64_t, LeaderSkillCalcFunc> LEADER_SKILL_CALC_FUNC = {
    {1, inc_smile},          {2, inc_pure},          {3, inc_cool},
    {112, inc_pure_by_smile}, {113, inc_cool_by_smile}, {121, inc_smile_by_pure},
    {123, inc_cool_by_pure},  {131, inc_smile_by_cool}, {132, inc_pure_by_cool},
};

Bonus calculate_bonus(int64_t leader_skill_effect_type, int64_t effect_value, int64_t smile, int64_t pure,
                      int64_t cool) {
    auto it = LEADER_SKILL_CALC_FUNC.find(leader_skill_effect_type);
    LeaderSkillCalcFunc func = it == LEADER_SKILL_CALC_FUNC.end() ? return_zero : it->second;
    return func(smile, pure, cool, (double)effect_value / 100);
}

}  // namespace npps::leader_skill
