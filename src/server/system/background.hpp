#pragma once
// npps4/system/background.py
#include "app/context.hpp"

#include <vector>

namespace npps::system::background {

bool has_background(Context &context, db::main::User &user, int64_t background_id);
bool unlock_background(Context &context, db::main::User &user, int64_t background_id, bool set_active = false);
std::vector<db::main::Background *> get_backgrounds(Context &context, db::main::User &user);
bool set_background_active(Context &context, db::main::User &user, int64_t background_id);
void init(Context &context, db::main::User &user);

}  // namespace npps::system::background
