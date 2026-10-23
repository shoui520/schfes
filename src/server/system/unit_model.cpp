// npps4/system/unit_model.py
#include "system/unit_model.hpp"

namespace npps::system::unit_model {

UnitExtraData get_extra_data(const UnitItem &item) {
    UnitExtraData data;
    data.level = item.level;
    data.exp = item.exp;
    data.rank = item.rank;
    data.love = item.love;
    data.skill_level = item.unit_skill_level;
    data.skill_exp = item.unit_skill_exp;
    data.unit_removable_skill_capacity = item.unit_removable_skill_capacity;
    data.display_rank = item.display_rank;
    data.is_signed = item.is_signed;
    data.removable_skill_ids = item.removable_skill_ids;
    return data;
}

}  // namespace npps::system::unit_model
