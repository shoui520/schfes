// npps4/system/item_model.py
#include "system/item_model.hpp"

#include "system/unit_model.hpp"

namespace npps::system::item_model {

model::Ptr<ItemExtraData> get_extra_data(const Item &item) {
    if (auto unit_item = dynamic_cast<const unit_model::UnitItem *>(&item))
        return model::make<unit_model::UnitExtraData>(unit_model::get_extra_data(*unit_item));
    return nullptr;
}

}  // namespace npps::system::item_model
