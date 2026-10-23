#pragma once
// npps4/system/unit_model.py. The models are generated in app/models.hpp; this header holds their methods.
#include "app/models.hpp"

namespace npps::system::unit_model {

// type AnyUnitItem = UnitItem | UnitSupportItem (UnitItem derives from UnitSupportItem).
using AnyUnitItem = model::Ptr<UnitSupportItem>;

// UnitExtraData.EMPTY
inline const UnitExtraData UnitExtraData_EMPTY{};

// Computed fields.
inline int64_t unit_id(const UnitSupportItem &item) { return item.item_id; }   // UnitSupportItem.unit_id
inline int64_t skill_level(const UnitInfoBase &info) { return info.unit_skill_level; }  // UnitInfoBase.skill_level
inline int64_t skill_level(const UnitItem &item) { return item.unit_skill_level; }

// UnitItem.get_extra_data()
UnitExtraData get_extra_data(const UnitItem &item);

}  // namespace npps::system::unit_model
