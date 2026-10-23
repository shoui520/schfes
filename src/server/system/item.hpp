#pragma once
// npps4/system/item.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <tuple>
#include <vector>

namespace npps::system::item {

int64_t get_item_category_for_type_1000(Context &context, int64_t item_id);
int64_t get_item_category(Context &context, const model::Ptr<item_model::Item> &item_data);
// Sets item_category_id in place and returns the same pointer.
model::Ptr<item_model::Item> update_item_category_id(Context &context, const model::Ptr<item_model::Item> &item_data);

model::Ptr<item_model::Item> loveca(int64_t amount, bool paid = false);
model::Ptr<item_model::Item> game_coin(int64_t amount);
model::Ptr<item_model::Item> social_point(int64_t amount);
model::Ptr<item_model::BaseItem> base_loveca(int64_t amount, bool paid = false);

std::vector<int64_t> get_buff_item_ids(Context &context);
std::vector<int64_t> get_reinforce_item_ids(Context &context);
// (general_item_list, buff_item_list, reinforce_item_list)
std::tuple<std::vector<common::ItemCount>, std::vector<common::ItemCount>, std::vector<common::ItemCount>>
get_item_list(Context &context, db::main::User &user);

// Null if the user has no row for item_id.
db::main::Item *get_item_data(Context &context, db::main::User &user, int64_t item_id);
db::main::Item *get_item_data_guaranteed(Context &context, db::main::User &user, int64_t item_id);
void add_item(Context &context, db::main::User &user, int64_t item_id, int64_t amount);
int64_t get_item_count(Context &context, db::main::User &user, int64_t item_id);

// Null if the user has no row for recovery_item_id.
db::main::RecoveryItem *get_recovery_item_data(Context &context, db::main::User &user, int64_t recovery_item_id);
db::main::RecoveryItem *get_recovery_item_data_guaranteed(Context &context, db::main::User &user,
                                                          int64_t recovery_item_id);
void add_recovery_item(Context &context, db::main::User &user, int64_t recovery_item_id, int64_t amount);
std::vector<common::ItemCount> get_recovery_items(Context &context, db::main::User &user);
// @context_cacheable("recovery_item"); null if missing or encrypted.
const db::item::RecoveryItem *get_recovery_item_info(Context &context, int64_t recovery_item_id);

}  // namespace npps::system::item
