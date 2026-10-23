// npps4/system/background.py
#include "system/background.hpp"

namespace npps::system::background {

bool has_background(Context &context, db::main::User &user, int64_t background_id) {
    return context.db.main.first<db::main::Background>("WHERE \"user_id\" = ? AND \"background_id\" = ?", user.id,
                                                       background_id) != nullptr;
}

bool unlock_background(Context &context, db::main::User &user, int64_t background_id, bool set_active) {
    if (has_background(context, user, background_id)) return false;

    db::main::Background bg;
    bg.user_id = user.id;
    bg.background_id = background_id;
    context.db.main.add(bg);
    context.db.main.flush();

    if (set_active) set_background_active(context, user, background_id);

    return true;
}

std::vector<db::main::Background *> get_backgrounds(Context &context, db::main::User &user) {
    return context.db.main.select<db::main::Background>("WHERE \"user_id\" = ?", user.id);
}

bool set_background_active(Context &context, db::main::User &user, int64_t background_id) {
    if (!has_background(context, user, background_id)) return false;

    user.active_background = background_id;
    context.db.main.flush();
    return true;
}

void init(Context &context, db::main::User &user) { unlock_background(context, user, 1, true); }

}  // namespace npps::system::background
