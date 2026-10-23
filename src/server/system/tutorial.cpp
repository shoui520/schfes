// npps4/system/tutorial.py
#include "system/tutorial.hpp"

#include "system/unit.hpp"
#include "system/user.hpp"

namespace npps::system::tutorial {

void phase1(Context &, db::main::User &u) { u.tutorial_state = 1; }

void phase2(Context &, db::main::User &u) { u.tutorial_state = 2; }

void phase3(Context &context, db::main::User &u) {
    const auto &game_setting = user::game_setting();
    // G +36400 + 600
    u.game_coin = u.game_coin + game_setting.initial_game_coin + 600;
    // Friend Points +5
    u.social_point = u.social_point + game_setting.live_social_point_for_others;
    // Add EXP
    user::add_exp(context, u, 11);
    // Reine Saeki
    unit::add_unit_simple(context, u, 13, true);
    // Akemi Kikuchi
    unit::add_unit_simple(context, u, 9, true);
    // Bond calculation
    unit::add_love_by_deck(context, u, u.active_deck_index, 34);
    u.tutorial_state = 3;
}

void finalize(Context &, db::main::User &u) { u.tutorial_state = -1; }

}  // namespace npps::system::tutorial
