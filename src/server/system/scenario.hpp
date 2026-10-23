#pragma once
// npps4/system/scenario.py
#include "app/context.hpp"

#include <vector>

namespace npps::system::scenario {

void init(Context &context, db::main::User &user);
bool unlock(Context &context, db::main::User &user, int64_t scenario_id);
// Null if not unlocked.
db::main::Scenario *get(Context &context, db::main::User &user, int64_t scenario_id);
std::vector<db::main::Scenario *> get_all(Context &context, db::main::User &user);
// @context_cacheable("scenario_valid")
bool valid(Context &context, int64_t scenario_id);
bool is_unlocked(Context &context, db::main::User &user, int64_t scenario_id);
bool is_completed(Context &context, db::main::User &user, int64_t scenario_id);
bool complete(Context &context, db::main::User &user, int64_t scenario_id);
int64_t count_completed(Context &context, db::main::User &user);
int64_t count(Context &context, db::main::User &user);

}  // namespace npps::system::scenario
