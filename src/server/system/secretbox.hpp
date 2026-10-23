#pragma once
// npps4/system/secretbox.py
#include "app/context.hpp"
#include "app/models.hpp"
#include "data/data.hpp"

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace npps::system::secretbox {

int64_t encode_cost_id(int64_t secretbox_id, int64_t button_index, int64_t cost_index);
// (secretbox_id, button_index, cost_index)
std::tuple<int64_t, int64_t, int64_t> decode_cost_id(int64_t cost_id);

// SecretboxAllButton or SecretboxAllButtonWithBaloon. ids = (secretbox_id, button_index).
model::Ptr<secretbox_model::SecretboxAllButton> query_secretbox_button(Context &context, db::main::User &user,
                                                                       const data::schema::SecretboxButton &button,
                                                                       const std::string &secretbox_name,
                                                                       std::tuple<int64_t, int64_t> ids);
std::vector<model::Ptr<secretbox_model::SecretboxAllButton>> get_secretbox_button_response(
    Context &context, db::main::User &target_user, const data::schema::SecretboxData &secretbox);
model::Ptr<secretbox_model::SecretboxAllSecretboxInfo> get_secretbox_info_response(
    Context &context, db::main::User &target_user, const data::schema::SecretboxData &secretbox, bool can_do_more);
std::vector<secretbox_model::SecretboxAllMemberCategory> get_all_secretbox_data_response(Context &context,
                                                                                         db::main::User &target_user);

// Throws std::out_of_range (KeyError) for an unknown id.
const data::schema::SecretboxData &get_secretbox_data(int64_t secretbox_id);

// Returns unit_ids.
std::vector<int64_t> roll_units(int64_t secretbox_id, int64_t amount, int64_t guarantee_rarity = 0,
                                int64_t guarantee_amount = 0,
                                const std::optional<std::vector<int64_t>> &rate_modifier = std::nullopt);

// button_index is 1-based (Python list indexing, so 0 means the last button).
const data::schema::SecretboxButton &get_secretbox_button(const data::schema::SecretboxData &secretbox_data,
                                                          int64_t button_index);
const data::schema::SecretboxButton &get_secretbox_button(int64_t secretbox_id, int64_t button_index);

int64_t get_user_currency(Context &context, db::main::User &target_user, const_::SECRETBOX_COST_TYPE cost_type,
                          std::optional<int64_t> cost_item_id);
void sub_user_currency(Context &context, db::main::User &target_user, const_::SECRETBOX_COST_TYPE cost_type,
                       std::optional<int64_t> cost_item_id, int64_t amount);

}  // namespace npps::system::secretbox
