#pragma once
// npps4/system/tos.py
#include "app/context.hpp"

namespace npps::system::tos {

bool is_agreed(Context &context, db::main::User &user, int64_t tos_id);
void agree(Context &context, db::main::User &user, int64_t tos_id);

}  // namespace npps::system::tos
