// npps4/system/item.py
#include "system/item.hpp"

#include "system/common.hpp"
#include "system/item_model.hpp"

#include <set>

namespace npps::system::item {

int64_t get_item_category_for_type_1000(Context &context, int64_t item_id) {
    const db::item::KGItem *item_info = context.db.item.get<db::item::KGItem>(item_id);
    if (item_info == nullptr) return 0;
    return item_info->item_category_id.value_or(0);
}

int64_t get_item_category(Context &context, const model::Ptr<item_model::Item> &item_data) {
    switch (item_data->add_type) {
    case const_::ADD_TYPE::ITEM: return get_item_category_for_type_1000(context, item_data->item_id);
    case const_::ADD_TYPE::GAME_COIN: return get_item_category_for_type_1000(context, 3);
    case const_::ADD_TYPE::LOVECA: return get_item_category_for_type_1000(context, 4);
    case const_::ADD_TYPE::SOCIAL_POINT: return get_item_category_for_type_1000(context, 5);
    default: return 0;
    }
}

model::Ptr<item_model::Item> update_item_category_id(Context &context, const model::Ptr<item_model::Item> &item_data) {
    item_data->item_category_id = get_item_category(context, item_data);
    return item_data;
}

static model::Ptr<item_model::Item> make_item(const_::ADD_TYPE add_type, int64_t item_id, int64_t amount,
                                              int64_t item_category_id) {
    auto result = model::make<item_model::Item>();
    result->add_type = add_type;
    result->item_id = item_id;
    result->amount = amount;
    result->item_category_id = item_category_id;
    return result;
}

model::Ptr<item_model::Item> loveca(int64_t amount, bool paid) {
    return make_item(const_::ADD_TYPE::LOVECA, paid ? 1 : 4, amount, 4);
}

model::Ptr<item_model::Item> game_coin(int64_t amount) {
    return make_item(const_::ADD_TYPE::GAME_COIN, 3, amount, 3);
}

model::Ptr<item_model::Item> social_point(int64_t amount) {
    return make_item(const_::ADD_TYPE::SOCIAL_POINT, 2, amount, 2);
}

model::Ptr<item_model::BaseItem> base_loveca(int64_t amount, bool paid) {
    auto result = model::make<item_model::BaseItem>();
    result->add_type = const_::ADD_TYPE::LOVECA;
    result->item_id = paid ? 1 : 4;
    result->amount = amount;
    return result;
}

std::vector<int64_t> get_buff_item_ids(Context &context) {
    std::vector<int64_t> result;
    for (const auto *i : context.db.item.select<db::item::BuffItem>("")) result.push_back(i->item_id);
    return result;
}

std::vector<int64_t> get_reinforce_item_ids(Context &context) {
    std::vector<int64_t> result;
    for (const auto *i : context.db.item.select<db::item::UnitReinforceItem>("")) result.push_back(i->item_id);
    return result;
}

std::tuple<std::vector<common::ItemCount>, std::vector<common::ItemCount>, std::vector<common::ItemCount>>
get_item_list(Context &context, db::main::User &user) {
    std::vector<int64_t> buff_ids = get_buff_item_ids(context);
    std::vector<int64_t> reinforce_ids = get_reinforce_item_ids(context);
    std::set<int64_t> buff_items(buff_ids.begin(), buff_ids.end());
    std::set<int64_t> reinforce_items(reinforce_ids.begin(), reinforce_ids.end());

    std::vector<common::ItemCount> general_item_list, buff_item_list, reinforce_item_list;

    for (const db::main::Item *i :
         context.db.main.select<db::main::Item>("WHERE \"user_id\" = ? AND \"amount\" > 0", user.id)) {
        common::ItemCount item_count = common::ItemCount::make(i->item_id, i->amount);

        if (reinforce_items.count(i->item_id)) reinforce_item_list.push_back(item_count);
        else if (buff_items.count(i->item_id)) buff_item_list.push_back(item_count);
        else general_item_list.push_back(item_count);
    }

    return {std::move(general_item_list), std::move(buff_item_list), std::move(reinforce_item_list)};
}

db::main::Item *get_item_data(Context &context, db::main::User &user, int64_t item_id) {
    return context.db.main.first<db::main::Item>("WHERE \"user_id\" = ? AND \"item_id\" = ?", user.id, item_id);
}

db::main::Item *get_item_data_guaranteed(Context &context, db::main::User &user, int64_t item_id) {
    db::main::Item *item_data = get_item_data(context, user, item_id);
    if (item_data == nullptr) {
        db::main::Item row;
        row.user_id = user.id;
        row.item_id = item_id;
        item_data = context.db.main.add(row);
    }
    return item_data;
}

void add_item(Context &context, db::main::User &user, int64_t item_id, int64_t amount) {
    switch (item_id) {
    case 2: user.social_point = user.social_point + amount; break;
    case 3: user.game_coin = user.game_coin + amount; break;
    case 4: user.free_sns_coin = user.free_sns_coin + amount; break;
    default: {
        db::main::Item *item_data = get_item_data_guaranteed(context, user, item_id);
        item_data->amount = item_data->amount + amount;
    }
    }
}

int64_t get_item_count(Context &context, db::main::User &user, int64_t item_id) {
    switch (item_id) {
    case 2: return user.social_point;
    case 3: return user.game_coin;
    case 4: return user.free_sns_coin + user.paid_sns_coin;
    default: {
        db::main::Item *item_data = get_item_data(context, user, item_id);
        return item_data == nullptr ? 0 : item_data->amount;
    }
    }
}

db::main::RecoveryItem *get_recovery_item_data(Context &context, db::main::User &user, int64_t recovery_item_id) {
    return context.db.main.first<db::main::RecoveryItem>("WHERE \"user_id\" = ? AND \"item_id\" = ?", user.id,
                                                         recovery_item_id);
}

db::main::RecoveryItem *get_recovery_item_data_guaranteed(Context &context, db::main::User &user,
                                                          int64_t recovery_item_id) {
    db::main::RecoveryItem *item_data = get_recovery_item_data(context, user, recovery_item_id);
    if (item_data == nullptr) {
        db::main::RecoveryItem row;
        row.user_id = user.id;
        row.item_id = recovery_item_id;
        item_data = context.db.main.add(row);
    }
    return item_data;
}

void add_recovery_item(Context &context, db::main::User &user, int64_t recovery_item_id, int64_t amount) {
    db::main::RecoveryItem *item_data = get_recovery_item_data_guaranteed(context, user, recovery_item_id);
    item_data->amount = item_data->amount + amount;
}

std::vector<common::ItemCount> get_recovery_items(Context &context, db::main::User &user) {
    std::vector<common::ItemCount> result;
    for (const db::main::RecoveryItem *i :
         context.db.main.select<db::main::RecoveryItem>("WHERE \"user_id\" = ? AND \"amount\" > 0", user.id))
        result.push_back(common::ItemCount::make(i->item_id, i->amount));
    return result;
}

const db::item::RecoveryItem *get_recovery_item_info(Context &context, int64_t recovery_item_id) {
    return common::get_cached<const db::item::RecoveryItem *>(
        context, "recovery_item", recovery_item_id, [](Context &c, int64_t id) {
            return db::get_decrypted_row<db::item::RecoveryItem>(c.db.item, id);
        });
}

}  // namespace npps::system::item
