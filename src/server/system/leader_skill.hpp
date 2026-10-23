#pragma once
// npps4/leader_skill.py
#include <cstdint>
#include <map>
#include <tuple>

namespace npps::leader_skill {

// (smile, pure, cool)
using Bonus = std::tuple<int64_t, int64_t, int64_t>;
using LeaderSkillCalcFunc = Bonus (*)(int64_t smile, int64_t pure, int64_t cool, double by);

Bonus inc_smile(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_pure(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_cool(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_smile_by_pure(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_smile_by_cool(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_pure_by_smile(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_pure_by_cool(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_cool_by_smile(int64_t smile, int64_t pure, int64_t cool, double by);
Bonus inc_cool_by_pure(int64_t smile, int64_t pure, int64_t cool, double by);

extern const std::map<int64_t, LeaderSkillCalcFunc> LEADER_SKILL_CALC_FUNC;

Bonus calculate_bonus(int64_t leader_skill_effect_type, int64_t effect_value, int64_t smile, int64_t pure,
                      int64_t cool);

}  // namespace npps::leader_skill
