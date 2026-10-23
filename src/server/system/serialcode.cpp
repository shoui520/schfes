// npps4/serialcode/__init__.py and npps4/serialcode/func.py
#include "system/serialcode.hpp"

#include "data/schema.hpp"
#include "system/advanced.hpp"
#include "system/reward.hpp"

#include <stdexcept>

namespace npps::serialcode {

namespace func {

std::optional<std::string> give_all_supporter_units(Context &context, db::main::User &user) {
    auto units = context.db.unit.select<db::unit::Unit>("WHERE disable_rank_up > 0 AND disable_rank_up < 5");
    std::vector<int64_t> unit_ids;
    for (const auto *u : units) unit_ids.push_back(u->unit_id);
    for (int64_t unit_id : unit_ids) {
        auto base_item = system::item_model::BaseItem::make(const_::ADD_TYPE::UNIT, unit_id);
        base_item.amount = 100;
        auto item_data = system::advanced::deserialize_item_data(context, base_item);
        system::reward::add_item(context, user, item_data, "追いかける, ショー・ヘーレーション!",
                                 std::string("Oikakeru, Snow Halation!"));
    }

    return std::string("Given all supporter members (100x quantity each).");
}

}  // namespace func

const std::map<std::string, Function> functions = {{"give_all_supporter_units", func::give_all_supporter_units}};

// util.java_hash_code(text, 0x7FFFFFFFFFFFFFFF); the 64-bit wrap keeps the low 63 bits exact.
static int64_t java_hash_code_63(const std::string &text) {
    uint64_t h = 0;
    const unsigned char *p = (const unsigned char *)text.data(), *end = p + text.size();
    while (p < end) {
        uint32_t cp = *p++;
        int extra = cp >= 0xf0 ? 3 : cp >= 0xe0 ? 2 : cp >= 0xc0 ? 1 : 0;
        if (extra) cp &= 0x3f >> extra;
        while (extra-- && p < end) cp = cp << 6 | (*p++ & 0x3f);
        h = (h * 31 + cp) & 0x7FFFFFFFFFFFFFFFULL;
    }
    return (int64_t)h;
}

std::string execute(Context &context, db::main::User &user, const std::string &input_code,
                    const data::schema::SerialCode &serial_code) {
    // Check amount
    int64_t *tracker_count = nullptr;
    if (serial_code.usage_limit) {
        int64_t serial_code_id = java_hash_code_63(serial_code.usage_limit->id);
        if (serial_code.usage_limit->global_) {
            auto *tracker = context.db.main.first<db::main::GlobalSerialCodeUsage>("WHERE serial_code_id = ?", serial_code_id);
            if (!tracker) {
                db::main::GlobalSerialCodeUsage t;
                t.serial_code_id = serial_code_id;
                t.count = 0;
                tracker = context.db.main.add(t);
            }
            tracker_count = &tracker->count;
        } else {
            auto *tracker = context.db.main.first<db::main::LocalSerialCodeUsage>("WHERE user_id = ? AND serial_code_id = ?",
                                                                                  user.id, serial_code_id);
            if (!tracker) {
                db::main::LocalSerialCodeUsage t;
                t.user_id = user.id;
                t.serial_code_id = serial_code_id;
                t.count = 0;
                tracker = context.db.main.add(t);
            }
            tracker_count = &tracker->count;
        }

        if (*tracker_count >= serial_code.usage_limit->amount) throw std::invalid_argument("serial code usage exceeded");
    }

    data::schema::SerialCodeAction action = data::schema::get_action(serial_code, input_code);
    std::string result_str;
    if (auto *give = std::get_if<data::schema::SerialCodeGiveItem>(&action)) {
        std::vector<std::string> given_item = {"Successfully given these items:"};
        for (const auto &item_data_serialized : give->items) {
            auto item_data = system::advanced::deserialize_item_data(context, *item_data_serialized);
            system::reward::add_item(context, user, item_data, give->message_jp, give->message_en);
            given_item.push_back(std::to_string(item_data->amount) + "x " + system::advanced::get_item_name(context, item_data));
        }
        for (size_t i = 0; i < given_item.size(); i++) result_str += (i ? "\n" : "") + given_item[i];
    } else {
        const auto &run = std::get<data::schema::SerialCodeRunFunction>(action);
        auto it = functions.find(run.function);
        if (it == functions.end()) {
            if (serial_code.action.is_array()) throw std::invalid_argument("cannot find specified function");
            throw std::invalid_argument("cannot find '" + run.function + "' function");
        }
        auto r = it->second(context, user);
        result_str = r && !r->empty() ? *r : "Serial code successfully executed.";
    }

    if (tracker_count) {
        *tracker_count = *tracker_count + 1;
        context.db.main.flush();
    }

    return result_str;
}

}  // namespace npps::serialcode
