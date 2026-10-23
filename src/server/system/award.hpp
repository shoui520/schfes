#pragma once
// npps4/system/award.py
#include "app/context.hpp"

#include <vector>

namespace npps::system::award {

bool has_award(Context &context, db::main::User &user, int64_t award_id);
bool unlock_award(Context &context, db::main::User &user, int64_t award_id, bool set_active = false);
std::vector<db::main::Award *> get_awards(Context &context, db::main::User &user);
bool set_award_active(Context &context, db::main::User &user, int64_t award_id);
void init(Context &context, db::main::User &user);

}  // namespace npps::system::award
