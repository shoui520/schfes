// npps4/system/removable_skill.py
#include "system/removable_skill.hpp"

#include "system/unit.hpp"

#include <cmath>

namespace npps::system::removable_skill {

std::tuple<int64_t, int64_t, int64_t> apply_stats(int64_t effect_type, double effect_value, bool fixed, int64_t smile,
                                                  int64_t pure, int64_t cool) {
    switch (effect_type) {
    case 1:
        if (fixed) return {(int64_t)effect_value, 0, 0};
        return {(int64_t)std::ceil((double)smile * effect_value / 100.0), 0, 0};
    case 2:
        if (fixed) return {0, (int64_t)effect_value, 0};
        return {0, (int64_t)std::ceil((double)pure * effect_value / 100.0), 0};
    case 3:
        if (fixed) return {0, 0, (int64_t)effect_value};
        return {0, 0, (int64_t)std::ceil((double)cool * effect_value / 100.0)};
    default: break;
    }

    return {0, 0, 0};
}

bool can_apply(Context &context, int64_t trigger_reference_type, int64_t trigger_type,
               const std::vector<db::main::Unit *> &player_units) {
    switch (trigger_reference_type) {
    case 0: return true;
    case 4:
        for (const db::main::Unit *unit_data : player_units) {
            const db::unit::Unit *unit_info = unit::get_unit_info(context, unit_data->unit_id);

            if (!unit::unit_type_has_tag(context, unit_info->unit_type_id, trigger_type)) return false;
        }

        return true;
    default: return false;
    }
}

bool can_apply_to_unit(Context &context, const db::unit::RemovableSkill &removable_skill,
                       const db::unit::Unit &unit_info) {
    switch (removable_skill.target_reference_type) {
    case 0: return true;
    case 1: return unit::unit_type_has_tag(context, unit_info.unit_type_id, removable_skill.target_type);
    case 2: return unit_info.unit_type_id == removable_skill.target_type;
    case 3: return unit_info.attribute_id == removable_skill.target_type;
    default: return false;
    }
}

}  // namespace npps::system::removable_skill
