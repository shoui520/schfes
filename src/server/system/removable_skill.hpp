#pragma once
// npps4/system/removable_skill.py
#include "app/context.hpp"

#include <tuple>
#include <vector>

namespace npps::system::removable_skill {

std::tuple<int64_t, int64_t, int64_t> apply_stats(int64_t effect_type, double effect_value, bool fixed, int64_t smile,
                                                  int64_t pure, int64_t cool);

bool can_apply(Context &context, int64_t trigger_reference_type, int64_t trigger_type,
               const std::vector<db::main::Unit *> &player_units);

bool can_apply_to_unit(Context &context, const db::unit::RemovableSkill &removable_skill,
                       const db::unit::Unit &unit_info);

}  // namespace npps::system::removable_skill
