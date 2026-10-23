#pragma once
// npps4/system/exchange.py. ExchangePointInfo, ExchangeCost and the ExchangeItem* models are
// generated in app/models.hpp.
#include "app/context.hpp"
#include "app/models.hpp"

#include <optional>
#include <vector>

namespace npps::system::exchange {

// @context_cacheable("exchange_festival_point_unit")
bool is_festival_unit(Context &context, int64_t unit_id);
// @context_cacheable("exchange_no_point_unit")
bool should_give_sticker(Context &context, int64_t unit_id);

// Null only when ensure is false and the user has no row.
db::main::ExchangePointItem *get_exchange_point(Context &context, db::main::User &user, int64_t exchange_point_id,
                                                bool ensure);
std::vector<db::main::ExchangePointItem *> get_exchange_point_list(Context &context, db::main::User &user);
int64_t get_exchange_point_amount(Context &context, db::main::User &user, int64_t exchange_point_id);
// Throw std::invalid_argument when quantity < 1.
bool add_exchange_point(Context &context, db::main::User &user, int64_t exchange_point_id, int64_t quantity);
bool sub_exchange_point(Context &context, db::main::User &user, int64_t exchange_point_id, int64_t quantity = 1);
std::vector<ExchangePointInfo> get_exchange_points_response(Context &context, db::main::User &user);
int64_t get_exchange_needed_to_idolize(Context &context, int64_t exchange_point_id, int64_t unit_rarity_id);
void add_exchange_item_got_count(Context &context, db::main::User &user, int64_t exchange_item_id, int64_t amount);

// Returns an ExchangeItemBase, ExchangeItemWithExpiry, ExchangeItemWithMaxCount or
// ExchangeItemWithMaxCountAndExpiry.
model::Ptr<ExchangeItemBase> get_exchange_item_info_by_raw_info(Context &context, db::main::User &user,
                                                                const data::schema::StickerShop &raw_info,
                                                                std::optional<int64_t> time = std::nullopt);
std::vector<model::Ptr<ExchangeItemBase>> get_exchange_item_info(Context &context, db::main::User &user);
// Points into the current server data (data::get()); null if not found.
const data::schema::StickerShop *find_raw_exchange_item_info_by_id(Context &context, int64_t exchange_item_id);

}  // namespace npps::system::exchange
