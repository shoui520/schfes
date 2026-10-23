// npps4/system/scenario.py
#include "system/scenario.hpp"

#include "system/common.hpp"

namespace npps::system::scenario {

void init(Context &context, db::main::User &user) {
    for (int64_t i = 1; i < 189; i++) {
        if (i >= 4 && i < 184) continue;
        db::main::Scenario sc;
        sc.user_id = user.id;
        sc.scenario_id = i;
        sc.completed = true;
        context.db.main.add(sc);
    }

    context.db.main.flush();
}

bool unlock(Context &context, db::main::User &user, int64_t scenario_id) {
    if (!is_unlocked(context, user, scenario_id)) {
        db::main::Scenario sc;
        sc.user_id = user.id;
        sc.scenario_id = scenario_id;
        sc.completed = false;
        context.db.main.add(sc);
        context.db.main.flush();
        return true;
    }

    return false;
}

db::main::Scenario *get(Context &context, db::main::User &user, int64_t scenario_id) {
    return context.db.main.first<db::main::Scenario>("WHERE \"user_id\" = ? AND \"scenario_id\" = ?", user.id,
                                                     scenario_id);
}

std::vector<db::main::Scenario *> get_all(Context &context, db::main::User &user) {
    return context.db.main.select<db::main::Scenario>("WHERE \"user_id\" = ?", user.id);
}

bool valid(Context &context, int64_t scenario_id) {
    return common::get_cached<bool>(context, "scenario_valid", scenario_id, [](Context &c, int64_t id) {
        return c.db.scenario.get<db::scenario::Scenario>(id) != nullptr;
    });
}

bool is_unlocked(Context &context, db::main::User &user, int64_t scenario_id) {
    return get(context, user, scenario_id) != nullptr;
}

bool is_completed(Context &context, db::main::User &user, int64_t scenario_id) {
    db::main::Scenario *sc = get(context, user, scenario_id);
    if (sc != nullptr) return sc->completed;
    return false;
}

bool complete(Context &context, db::main::User &user, int64_t scenario_id) {
    db::main::Scenario *sc = get(context, user, scenario_id);
    if (sc == nullptr || sc->completed) return false;

    sc->completed = true;
    context.db.main.flush();
    return true;
}

int64_t count_completed(Context &context, db::main::User &user) {
    return context.db.main.count<db::main::Scenario>("WHERE \"user_id\" = ? AND \"completed\" = 1", user.id);
}

int64_t count(Context &context, db::main::User &user) {
    return context.db.main.count<db::main::Scenario>("WHERE \"user_id\" = ?", user.id);
}

}  // namespace npps::system::scenario
