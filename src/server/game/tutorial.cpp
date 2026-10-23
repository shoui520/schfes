// npps4/game/tutorial.py
#include "app/idol.hpp"
#include "system/tutorial.hpp"
#include "system/user.hpp"

namespace npps::game::tutorial {

static void tutorial_progress(Context &context, const TutorialProgressRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    if (current_user->tutorial_state == -1) throw error::IdolError(200, 720, "Tutorial already finished");

    if (current_user->tutorial_state == 0 && request.tutorial_state == 1) {
        system::tutorial::phase1(context, *current_user);
    } else if (current_user->tutorial_state == 1 && request.tutorial_state == 2) {
        system::tutorial::phase2(context, *current_user);
    } else if (current_user->tutorial_state == 2 && request.tutorial_state == 3) {
        system::tutorial::phase3(context, *current_user);
    } else if (current_user->tutorial_state == 3 && request.tutorial_state == -1) {
        system::tutorial::finalize(context, *current_user);
    } else {
        throw error::IdolError(200, 720,
                               util::format("Unknown state, u %lld r %lld", (long long)current_user->tutorial_state,
                                            (long long)request.tutorial_state));
    }
}
NPPS_ENDPOINT("tutorial", "progress", User, tutorial_progress, .batchable = false);

static void tutorial_skip(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    if (current_user->tutorial_state == -1) throw error::IdolError(200, 720, "Tutorial already finished");

    switch (current_user->tutorial_state) {
    case 0:
        system::tutorial::phase1(context, *current_user);
        system::tutorial::phase2(context, *current_user);
        system::tutorial::phase3(context, *current_user);
        system::tutorial::finalize(context, *current_user);
        break;
    case 1:
        system::tutorial::phase2(context, *current_user);
        system::tutorial::phase3(context, *current_user);
        system::tutorial::finalize(context, *current_user);
        break;
    case 2:
        system::tutorial::phase3(context, *current_user);
        system::tutorial::finalize(context, *current_user);
        break;
    case 3: system::tutorial::finalize(context, *current_user); break;
    default:
        throw error::IdolError(200, 720,
                               util::format("Invalid tutorial state: %lld", (long long)current_user->tutorial_state));
    }
}
NPPS_ENDPOINT("tutorial", "skip", User, tutorial_skip, .batchable = false);

}  // namespace npps::game::tutorial
