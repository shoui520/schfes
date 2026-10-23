// npps4/game/item.py
#include "app/idol.hpp"
#include "system/item.hpp"
#include "system/user.hpp"

namespace npps::game::item {

static ItemListResponse item_list(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    auto [general_item_list, buff_item_list, reinforce_item_list] = system::item::get_item_list(context, *current_user);
    return ItemListResponse::make(general_item_list, buff_item_list, reinforce_item_list);
}
NPPS_ENDPOINT("item", "list", User, item_list);

}  // namespace npps::game::item
