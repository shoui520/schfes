#pragma once
// npps4/system/item_model.py. ItemExtraData, BaseItem and Item are generated in app/models.hpp.
#include "app/models.hpp"

namespace npps::system::item_model {

// Item.get_extra_data(); dispatches on the dynamic type (UnitItem returns a UnitExtraData).
// Returns null for a plain Item.
model::Ptr<ItemExtraData> get_extra_data(const Item &item);

}  // namespace npps::system::item_model
