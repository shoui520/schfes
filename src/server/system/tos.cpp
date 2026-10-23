// npps4/system/tos.py
#include "system/tos.hpp"

namespace npps::system::tos {

bool is_agreed(Context &context, db::main::User &user, int64_t tos_id) {
    return context.db.main.count<db::main::TOSAgree>("WHERE \"user_id\" = ? AND \"tos_id\" = ?", user.id, tos_id) > 0;
}

void agree(Context &context, db::main::User &user, int64_t tos_id) {
    db::main::TOSAgree row;
    row.user_id = user.id;
    row.tos_id = tos_id;
    context.db.main.add(row);
    context.db.main.flush();
}

}  // namespace npps::system::tos
