// npps4/system/subscenario.py
#include "system/subscenario.hpp"

#include "system/common.hpp"

namespace npps::system::subscenario {

bool unlock(Context &context, db::main::User &user, int64_t subscenario_id) {
    if (!is_unlocked(context, user, subscenario_id)) {
        db::main::SubScenario sc;
        sc.user_id = user.id;
        sc.subscenario_id = subscenario_id;
        sc.completed = false;
        context.db.main.add(sc);
        context.db.main.flush();
        return true;
    }

    return false;
}

db::main::SubScenario *get(Context &context, db::main::User &user, int64_t subscenario_id) {
    return context.db.main.first<db::main::SubScenario>("WHERE \"user_id\" = ? AND \"subscenario_id\" = ?", user.id,
                                                        subscenario_id);
}

std::vector<db::main::SubScenario *> get_all(Context &context, db::main::User &user) {
    return context.db.main.select<db::main::SubScenario>("WHERE \"user_id\" = ?", user.id);
}

bool valid(Context &context, int64_t subscenario_id) {
    return common::get_cached<bool>(context, "subscenario_valid", subscenario_id, [](Context &c, int64_t id) {
        return c.db.subscenario.get<db::subscenario::SubScenario>(id) != nullptr;
    });
}

bool is_unlocked(Context &context, db::main::User &user, int64_t subscenario_id) {
    return get(context, user, subscenario_id) != nullptr;
}

bool is_completed(Context &context, db::main::User &user, int64_t subscenario_id) {
    db::main::SubScenario *sc = get(context, user, subscenario_id);
    if (sc != nullptr) return sc->completed;
    return false;
}

bool complete(Context &context, db::main::User &user, int64_t subscenario_id) {
    db::main::SubScenario *sc = get(context, user, subscenario_id);
    if (sc == nullptr || sc->completed) return false;

    sc->completed = true;
    context.db.main.flush();
    return true;
}

int64_t get_subscenario_id_of_unit_id(Context &context, int64_t unit_id) {
    const db::subscenario::SubScenario *sc_info =
        db::decrypt_row(context.db.subscenario.first<db::subscenario::SubScenario>("WHERE \"unit_id\" = ?", unit_id));
    return sc_info != nullptr ? sc_info->subscenario_id : 0;
}

}  // namespace npps::system::subscenario
