// npps4/system/exchange.py
#include "system/exchange.hpp"

#include "data/data.hpp"
#include "system/award.hpp"
#include "system/background.hpp"
#include "system/common.hpp"

#include <algorithm>
#include <stdexcept>

namespace npps::system::exchange {

bool is_festival_unit(Context &context, int64_t unit_id) {
    return common::get_cached<bool>(context, "exchange_festival_point_unit", unit_id, [](Context &c, int64_t id) {
        return c.db.exchange.get<db::exchange::ExchangeFestivalPointUnit>(id) != nullptr;
    });
}

bool should_give_sticker(Context &context, int64_t unit_id) {
    return common::get_cached<bool>(context, "exchange_no_point_unit", unit_id, [](Context &c, int64_t id) {
        return c.db.exchange.get<db::exchange::ExchangeNoPointUnit>(id) == nullptr;
    });
}

db::main::ExchangePointItem *get_exchange_point(Context &context, db::main::User &user, int64_t exchange_point_id,
                                                bool ensure) {
    db::main::ExchangePointItem *exchange_point = context.db.main.first<db::main::ExchangePointItem>(
        "WHERE \"user_id\" = ? AND \"exchange_point_id\" = ?", user.id, exchange_point_id);

    if (exchange_point == nullptr && ensure) {
        db::main::ExchangePointItem row;
        row.user_id = user.id;
        row.exchange_point_id = exchange_point_id;
        exchange_point = context.db.main.add(row);
        context.db.main.flush();
    }

    return exchange_point;
}

std::vector<db::main::ExchangePointItem *> get_exchange_point_list(Context &context, db::main::User &user) {
    return context.db.main.select<db::main::ExchangePointItem>("WHERE \"user_id\" = ? AND \"amount\" > 0", user.id);
}

int64_t get_exchange_point_amount(Context &context, db::main::User &user, int64_t exchange_point_id) {
    db::main::ExchangePointItem *exchange_point = get_exchange_point(context, user, exchange_point_id, true);
    if (exchange_point == nullptr) return 0;

    return exchange_point->amount;
}

bool add_exchange_point(Context &context, db::main::User &user, int64_t exchange_point_id, int64_t quantity) {
    if (quantity < 1) throw std::invalid_argument("invalid amount");

    db::main::ExchangePointItem *exchange_point = get_exchange_point(context, user, exchange_point_id, true);
    if (exchange_point == nullptr) return false;

    exchange_point->amount = exchange_point->amount + quantity;
    context.db.main.flush();
    return true;
}

bool sub_exchange_point(Context &context, db::main::User &user, int64_t exchange_point_id, int64_t quantity) {
    if (quantity < 1) throw std::invalid_argument("invalid amount");

    db::main::ExchangePointItem *exchange_point = get_exchange_point(context, user, exchange_point_id, false);
    if (exchange_point != nullptr && exchange_point->amount >= quantity) {
        exchange_point->amount = exchange_point->amount - quantity;
        context.db.main.flush();
        return true;
    }

    return false;
}

std::vector<ExchangePointInfo> get_exchange_points_response(Context &context, db::main::User &user) {
    std::vector<ExchangePointInfo> result;
    for (const db::main::ExchangePointItem *e :
         context.db.main.select<db::main::ExchangePointItem>("WHERE \"user_id\" = ?", user.id))
        result.push_back(ExchangePointInfo::make(e->exchange_point_id, e->amount));
    return result;
}

int64_t get_exchange_needed_to_idolize(Context &context, int64_t exchange_point_id, int64_t unit_rarity_id) {
    if (unit_rarity_id < 2 || unit_rarity_id > 5) {
        // Cannot use any sticker to idolize N cards
        return 0;
    }

    const db::exchange::ExchangePoint *points = context.db.exchange.get<db::exchange::ExchangePoint>(exchange_point_id);
    if (points == nullptr) return 0;
    // r, sr, ur, ssr
    const std::optional<int64_t> values[4] = {points->r_rank_up_point, points->sr_rank_up_point,
                                              points->ur_rank_up_point, points->ssr_rank_up_point};
    return values[unit_rarity_id - 2].value_or(0);
}

static db::main::ExchangeItemLimit *get_exchange_item_limit(Context &context, db::main::User &user,
                                                            int64_t exchange_item_id, bool guarantee) {
    db::main::ExchangeItemLimit *exchange_limit = context.db.main.first<db::main::ExchangeItemLimit>(
        "WHERE \"user_id\" = ? AND \"exchange_item_id\" = ?", user.id, exchange_item_id);

    if (exchange_limit == nullptr && guarantee) {
        db::main::ExchangeItemLimit row;
        row.user_id = user.id;
        row.exchange_item_id = exchange_item_id;
        exchange_limit = context.db.main.add(row);
    }

    return exchange_limit;
}

void add_exchange_item_got_count(Context &context, db::main::User &user, int64_t exchange_item_id, int64_t amount) {
    db::main::ExchangeItemLimit *exchange_limit = get_exchange_item_limit(context, user, exchange_item_id, true);
    exchange_limit->count = exchange_limit->count + amount;
    context.db.main.flush();
}

model::Ptr<ExchangeItemBase> get_exchange_item_info_by_raw_info(Context &context, db::main::User &user,
                                                                const data::schema::StickerShop &raw_info,
                                                                std::optional<int64_t> time) {
    int64_t now = time ? *time : util::time();

    int64_t exchange_item_id = data::schema::exchange_item_id(raw_info);
    db::main::ExchangeItemLimit *exchange_limit = get_exchange_item_limit(context, user, exchange_item_id, false);
    std::string exchange_title = context.get_text(raw_info.name, raw_info.name_en);
    bool exchange_is_new = exchange_limit == nullptr;
    exchange_limit = get_exchange_item_limit(context, user, exchange_item_id, true);
    std::optional<int64_t> exchange_max_amount;
    int64_t exchange_got_item_count = exchange_limit->count;
    int64_t exchange_term_count = std::max<int64_t>(util::floordiv(raw_info.end_time - now + 86399, 86400), 0);
    if (raw_info.limit > 0) exchange_max_amount = raw_info.limit;

    // Handling special case
    switch (raw_info.add_type) {
    case const_::ADD_TYPE::AWARD:
        exchange_max_amount = 1;
        exchange_got_item_count = (int64_t)award::has_award(context, user, raw_info.item_id);
        break;
    case const_::ADD_TYPE::BACKGROUND:
        exchange_max_amount = 1;
        exchange_got_item_count = (int64_t)background::has_background(context, user, raw_info.item_id);
        break;
    default: break;
    }

    // Oh no
    ExchangeItemBase base;
    base.exchange_item_id = exchange_item_id;
    base.title = exchange_title;
    base.is_new = exchange_is_new;
    base.add_type = raw_info.add_type;
    base.item_id = raw_info.item_id;
    base.amount = raw_info.amount;
    // Client crash if you have more than 3
    for (size_t i = 0; i < raw_info.costs.size() && i < 3; i++)
        base.cost_list.push_back(ExchangeCost::make(raw_info.costs[i].rarity, raw_info.costs[i].cost));
    base.already_obtained = exchange_got_item_count > 0;
    base.got_item_count = exchange_got_item_count;
    base.term_count = exchange_term_count;

    if (exchange_max_amount) {
        if (raw_info.end_time > 0) {
            auto result = model::make<ExchangeItemWithMaxCountAndExpiry>();
            static_cast<ExchangeItemBase &>(*result) = base;
            result->term_end_date = util::timestamp_to_datetime(raw_info.end_time);
            result->max_item_count = *exchange_max_amount;
            return result;
        }
        auto result = model::make<ExchangeItemWithMaxCount>();
        static_cast<ExchangeItemBase &>(*result) = base;
        result->max_item_count = *exchange_max_amount;
        return result;
    }
    if (raw_info.end_time > 0) {
        auto result = model::make<ExchangeItemWithExpiry>();
        static_cast<ExchangeItemBase &>(*result) = base;
        result->term_end_date = util::timestamp_to_datetime(raw_info.end_time);
        return result;
    }
    return model::make<ExchangeItemBase>(base);
}

std::vector<model::Ptr<ExchangeItemBase>> get_exchange_item_info(Context &context, db::main::User &user) {
    const data::ServerData &server_data = data::get();
    std::vector<model::Ptr<ExchangeItemBase>> result;

    for (const data::schema::StickerShop &raw_info : server_data.sticker_shop) {
        int64_t time = util::time();

        if (raw_info.end_time == 0 || raw_info.end_time >= time)
            result.push_back(get_exchange_item_info_by_raw_info(context, user, raw_info, time));
    }

    return result;
}

const data::schema::StickerShop *find_raw_exchange_item_info_by_id(Context &, int64_t exchange_item_id) {
    const data::ServerData &server_data = data::get();

    for (const data::schema::StickerShop &raw_info : server_data.sticker_shop)
        if (data::schema::_internal_id(raw_info) == exchange_item_id) return &raw_info;

    return nullptr;
}

}  // namespace npps::system::exchange
