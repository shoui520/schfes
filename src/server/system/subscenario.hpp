#pragma once
// npps4/system/subscenario.py
#include "app/context.hpp"

#include <vector>

namespace npps::system::subscenario {

bool unlock(Context &context, db::main::User &user, int64_t subscenario_id);
// Null if not unlocked.
db::main::SubScenario *get(Context &context, db::main::User &user, int64_t subscenario_id);
std::vector<db::main::SubScenario *> get_all(Context &context, db::main::User &user);
// @context_cacheable("subscenario_valid")
bool valid(Context &context, int64_t subscenario_id);
bool is_unlocked(Context &context, db::main::User &user, int64_t subscenario_id);
bool is_completed(Context &context, db::main::User &user, int64_t subscenario_id);
bool complete(Context &context, db::main::User &user, int64_t subscenario_id);
// 0 if the unit has no side story.
int64_t get_subscenario_id_of_unit_id(Context &context, int64_t unit_id);

}  // namespace npps::system::subscenario
