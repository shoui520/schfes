// npps4/system/award.py
#include "system/award.hpp"

namespace npps::system::award {

bool has_award(Context &context, db::main::User &user, int64_t award_id) {
    return context.db.main.first<db::main::Award>("WHERE \"user_id\" = ? AND \"award_id\" = ?", user.id, award_id) !=
           nullptr;
}

bool unlock_award(Context &context, db::main::User &user, int64_t award_id, bool set_active) {
    if (has_award(context, user, award_id)) return false;

    db::main::Award bg;
    bg.user_id = user.id;
    bg.award_id = award_id;
    context.db.main.add(bg);
    context.db.main.flush();

    if (set_active) set_award_active(context, user, award_id);

    return true;
}

std::vector<db::main::Award *> get_awards(Context &context, db::main::User &user) {
    return context.db.main.select<db::main::Award>("WHERE \"user_id\" = ?", user.id);
}

bool set_award_active(Context &context, db::main::User &user, int64_t award_id) {
    if (!has_award(context, user, award_id)) return false;

    user.active_award = award_id;
    context.db.main.flush();
    return true;
}

void init(Context &context, db::main::User &user) {
    unlock_award(context, user, 1, true);
    unlock_award(context, user, 23);
}

}  // namespace npps::system::award
