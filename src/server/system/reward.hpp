#pragma once
// npps4/system/reward.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace npps::system::reward {

// time == 0 means util::time().
void cleanup_incentive(Context &context, int64_t time = 0);

// Background task (context.add_task(try_cleanup_incentive)); opens its own Context.
void try_cleanup_incentive();

db::main::Incentive *add_item(Context &context, db::main::User &user, const model::Ptr<item_model::Item> &item_data,
                              const std::string &reason_jp, const std::optional<std::string> &reason_en = std::nullopt,
                              int64_t expire = 0);

// apply_filter: returns the SQL condition (" AND ..." or empty) for `filter_config` and appends its parameters.
std::string apply_filter(const FilterConfig &filter_config, std::vector<int64_t> &params);

// RewardListRequest/RewardHistoryRequest are passed directly (they derive from FilterConfig).
std::vector<db::main::Incentive *> get_presentbox(Context &context, db::main::User &user,
                                                  const FilterConfig &filter_config, int64_t offset = 0,
                                                  int64_t limit = 1000, bool order_ascending = false,
                                                  bool order_expiry_date = false);

std::vector<db::main::Incentive *> get_presentbox_simple(Context &context, db::main::User &user);

// filter_config: None -> nullptr.
int64_t count_presentbox(Context &context, db::main::User &user, const FilterConfig *filter_config = nullptr);

// Returns common.AnyItem.
model::Ptr<item_model::Item> resolve_incentive(Context &context, db::main::User &user, db::main::Incentive &incentive);

// Null when not found.
db::main::Incentive *get_incentive(Context &context, db::main::User &user, int64_t incentive_id);

void remove_incentive(Context &context, db::main::Incentive &incentive);

bool has_at_least_one(Context &context, db::main::User &user, const_::ADD_TYPE add_type, int64_t item_id);

}  // namespace npps::system::reward
