#pragma once
// npps4/system/friend.py
#include "app/context.hpp"
#include "app/models.hpp"

namespace npps::system::friend_ {

const_::FRIEND_STATUS get_friend_status(Context &context, db::main::User &current_user, db::main::User &target_user);

}  // namespace npps::system::friend_
