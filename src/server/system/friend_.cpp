// npps4/system/friend.py
#include "system/friend_.hpp"

namespace npps::system::friend_ {

const_::FRIEND_STATUS get_friend_status(Context &, db::main::User &current_user, db::main::User &target_user) {
    // TODO
    if (current_user.id == target_user.id) return const_::FRIEND_STATUS::FRIEND;
    return const_::FRIEND_STATUS::OTHER;
}

}  // namespace npps::system::friend_
