// npps4/system/reward.py
#include "system/reward.hpp"

#include "system/advanced.hpp"
#include "system/item_model.hpp"
#include "system/live.hpp"

namespace npps::system::reward {

namespace {

// json.dumps(obj, separators=(",", ":"))
void dump_compact(const Json &j, std::string &out) {
    switch (j.type()) {
    case Json::Array: {
        out.push_back('[');
        bool first = true;
        for (const Json &e : j.elements()) {
            if (!first) out.push_back(',');
            first = false;
            dump_compact(e, out);
        }
        out.push_back(']');
        break;
    }
    case Json::Object: {
        out.push_back('{');
        bool first = true;
        for (const auto &m : j.members()) {
            if (!first) out.push_back(',');
            first = false;
            json_escape_to(out, m.first);
            out.push_back(':');
            dump_compact(m.second, out);
        }
        out.push_back('}');
        break;
    }
    default: j.dump_to(out); break;
    }
}

// Substitutes the `?` placeholders with integer literals (apply_filter parameters are integers only).
std::string inline_params(const std::string &sql, const std::vector<int64_t> &params) {
    std::string out;
    size_t next = 0;
    for (char c : sql) {
        if (c == '?') out += std::to_string(params.at(next++));
        else out.push_back(c);
    }
    return out;
}

const char *const NOT_EXPIRED = "(\"expire_date\" = 0 OR \"expire_date\" >= ?)";

bool currently_cleaning = false;

}  // namespace

void cleanup_incentive(Context &context, int64_t time) {
    if (time == 0) time = util::time();

    context.db.main.execute("DELETE FROM \"incentive\" WHERE \"expire_date\" != 0 AND \"expire_date\" < ?", time);
    context.db.main.flush();
}

void try_cleanup_incentive() {
    if (!currently_cleaning) {
        currently_cleaning = true;
        try {
            Context context;
            try {
                cleanup_incentive(context);
                context.commit();
            } catch (...) {
                context.rollback();
                throw;
            }
        } catch (...) {
            currently_cleaning = false;
            throw;
        }
        currently_cleaning = false;
    }
}

static void schedule_cleanup(Context &context) {
    if (context.support_background_task()) context.add_task([] { try_cleanup_incentive(); });
}

db::main::Incentive *add_item(Context &context, db::main::User &user, const model::Ptr<item_model::Item> &item_data,
                              const std::string &reason_jp, const std::optional<std::string> &reason_en,
                              int64_t expire) {
    schedule_cleanup(context);

    model::Ptr<item_model::ItemExtraData> extra_data = item_model::get_extra_data(*item_data);
    db::main::Incentive incentive;
    incentive.user_id = user.id;
    incentive.add_type = (int64_t)item_data->add_type;
    incentive.item_id = item_data->item_id;
    incentive.amount = item_data->amount;
    incentive.message_jp = reason_jp;
    incentive.message_en = reason_en;
    if (extra_data) {
        std::string dumped;
        dump_compact(extra_data->to_json(), dumped);
        incentive.extra_data = dumped;
    }
    incentive.expire_date = expire;

    if (auto support = std::dynamic_pointer_cast<unit_model::UnitSupportItem>(item_data)) {
        incentive.unit_attribute = support->attribute;
        incentive.unit_rarity = support->unit_rarity_id;
    }

    db::main::Incentive *added = context.db.main.add(incentive);
    context.db.main.flush();
    return added;
}

std::string apply_filter(const FilterConfig &filter_config, std::vector<int64_t> &params) {
    std::string cond;
    switch (filter_config.category) {
    case RewardCategory::MEMBERS:
        cond += " AND \"add_type\" = ?";
        params.push_back((int64_t)const_::ADD_TYPE::UNIT);
        if (filter_config.filter.at(0) > 0) {
            cond += " AND \"unit_rarity\" = ?";
            params.push_back(filter_config.filter[0]);
        }
        if (filter_config.filter.at(1) > 0) {
            cond += " AND \"unit_attribute\" = ?";
            params.push_back(filter_config.filter[1]);
        }
        // TODO: Not in album filter
        break;
    case RewardCategory::ITEMS:
        if (filter_config.filter.size() == 1 && filter_config.filter[0] == 0) {
            cond += " AND \"add_type\" != ?";
            params.push_back((int64_t)const_::ADD_TYPE::UNIT);
        } else {
            cond += " AND \"add_type\" IN (";
            for (size_t i = 0; i < filter_config.filter.size(); i++) {
                if (i) cond += ", ";
                cond += "?";
                params.push_back(filter_config.filter[i]);
            }
            cond += ")";
        }
        break;
    default: break;
    }
    return cond;
}

std::vector<db::main::Incentive *> get_presentbox(Context &context, db::main::User &user,
                                                  const FilterConfig &filter_config, int64_t offset, int64_t limit,
                                                  bool order_ascending, bool order_expiry_date) {
    schedule_cleanup(context);

    int64_t t = util::time();

    // Query non-expire incentives.
    std::vector<int64_t> params = {user.id, t};
    std::string tail = std::string("WHERE \"user_id\" = ? AND ") + NOT_EXPIRED;
    tail += apply_filter(filter_config, params);

    if (order_expiry_date) {
        if (order_ascending) {
            // When ordering by expiry date ascending, show no expiration last
            tail += " ORDER BY CASE WHEN \"expire_date\" = 0 THEN 1 ELSE 0 END, \"expire_date\" ASC, \"id\"";
        } else {
            // When ordering by expiry date descending, show no expiration first
            tail += " ORDER BY CASE WHEN \"expire_date\" = 0 THEN 0 ELSE 1 END, \"expire_date\" DESC, \"id\" DESC";
        }
    } else {
        tail += order_ascending ? " ORDER BY \"insert_date\" ASC, \"id\" DESC"
                                : " ORDER BY \"insert_date\" DESC, \"id\" DESC";
    }

    tail += " LIMIT " + std::to_string(limit > 0 ? limit : -1) + " OFFSET " + std::to_string(offset);
    return context.db.main.select<db::main::Incentive>(inline_params(tail, params));
}

std::vector<db::main::Incentive *> get_presentbox_simple(Context &context, db::main::User &user) {
    schedule_cleanup(context);

    int64_t t = util::time();

    // Query non-expire incentives.
    return context.db.main.select<db::main::Incentive>(std::string("WHERE \"user_id\" = ? AND ") + NOT_EXPIRED,
                                                       user.id, t);
}

int64_t count_presentbox(Context &context, db::main::User &user, const FilterConfig *filter_config) {
    schedule_cleanup(context);

    int64_t t = util::time();

    std::vector<int64_t> params = {user.id, t};
    std::string tail = std::string("WHERE \"user_id\" = ? AND ") + NOT_EXPIRED;
    if (filter_config) tail += apply_filter(*filter_config, params);
    return context.db.main.count<db::main::Incentive>(inline_params(tail, params));
}

model::Ptr<item_model::Item> resolve_incentive(Context &context, db::main::User &user,
                                               db::main::Incentive &incentive) {
    item_model::BaseItem base;
    base.add_type = (const_::ADD_TYPE)incentive.add_type;
    base.item_id = incentive.item_id;
    base.amount = incentive.amount;
    if (incentive.extra_data) base.extra_data = Json::parse(*incentive.extra_data);
    model::Ptr<item_model::Item> item_data = advanced::deserialize_item_data(context, base);
    if (auto live_item = std::dynamic_pointer_cast<live_model::LiveItem>(item_data)) {
        live_item->additional_normal_live_status_list =
            live::get_normal_live_clear_status_of_track(context, user, incentive.item_id);
        live_item->additional_training_live_status_list =
            live::get_training_live_clear_status_of_track(context, user, incentive.item_id);
    }
    return item_data;
}

db::main::Incentive *get_incentive(Context &context, db::main::User &user, int64_t incentive_id) {
    schedule_cleanup(context);

    return context.db.main.first<db::main::Incentive>("WHERE \"user_id\" = ? AND \"id\" = ?", user.id, incentive_id);
}

void remove_incentive(Context &context, db::main::Incentive &incentive) {
    // TODO: Move to incentive history
    context.db.main.remove(&incentive);
    context.db.main.flush();
}

bool has_at_least_one(Context &context, db::main::User &user, const_::ADD_TYPE add_type, int64_t item_id) {
    schedule_cleanup(context);

    return context.db.main.count<db::main::Incentive>("WHERE \"user_id\" = ? AND \"add_type\" = ? AND \"item_id\" = ?",
                                                      user.id, (int64_t)add_type, item_id) > 0;
}

}  // namespace npps::system::reward
