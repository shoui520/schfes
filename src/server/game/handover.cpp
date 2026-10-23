// npps4/game/handover.py
#include "app/idol.hpp"
#include "app/session.hpp"
#include "system/handover.hpp"
#include "system/user.hpp"

namespace npps::game::handover {

// TODO: Add into config.toml?
static const char *REROLL_TRANSFER_ID = "nil";
static const char *REROLL_TRANSFER_PASSCODE = "nil";

static const std::string &reroll_passcode() {
    static const std::string code =
        system::handover::generate_passcode_sha1(REROLL_TRANSFER_ID, REROLL_TRANSFER_PASSCODE);
    return code;
}

static void handover_exec(Context &context, const HandoverExecRequest &request) {
    // Special case for "nil", "nil":
    db::main::User *current_user = system::user::get_current(context);
    util::log("exec %s", request.to_json().dump().c_str());

    db::main::User *target_user = nullptr;
    if (request.handover_id == REROLL_TRANSFER_ID && request.handover_code == reroll_passcode()) {
        if (current_user->level == 1) {
            // Really?
            throw error::by_code(error::ERROR_HANDOVER_EXPIRE);
        }

        // Reroll
        if (!current_user->key || !current_user->passwd) throw std::logic_error("assertion failed");
        target_user = system::user::create(context, current_user->key, current_user->passwd);
    } else {
        target_user = system::handover::find_user_by_passcode(context, request.handover_code);
        if (target_user == nullptr) throw error::by_code(error::ERROR_HANDOVER_INVALID_ID_OR_CODE);
        // Sanity checks
        if (target_user->locked) throw error::by_code(error::ERROR_HANDOVER_LOCKED_USER);
        if (target_user->id == current_user->id) throw error::by_code(error::ERROR_HANDOVER_SELF);
    }

    system::handover::swap_credentials(*current_user, *target_user);
    target_user->transfer_sha1.reset();
    session::invalidate_current(context);
}
NPPS_ENDPOINT("handover", "exec", User, handover_exec, .batchable = false);

static KIDInfoResponse handover_kidinfo(Context &context) {
    // TODO
    util::log("handover.kidInfo stub: %s", context.raw_request_data.c_str());
    throw error::by_code(error::ERROR_KLAB_ID_SERVICE_MAINTENANCE);
}
NPPS_ENDPOINT("handover", "kidInfo", User, handover_kidinfo);

static KIDStatusResponse handover_kidstatus(Context &context) {
    // TODO
    util::log("handover.kidStatus stub: %s", context.raw_request_data.c_str());
    KIDStatusResponse response;
    response.has_klab_id = false;
    return response;
}
NPPS_ENDPOINT("handover", "kidStatus", User, handover_kidstatus);

static HandoverReserveTransferResponse handover_reservetransfer(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::string transfer_code = system::handover::generate_transfer_code();
    current_user->transfer_sha1 = system::handover::generate_passcode_sha1(current_user->invite_code, transfer_code);

    auto response = HandoverReserveTransferResponse::make(current_user->invite_code, transfer_code);
    response.insert_date = util::timestamp_to_datetime(util::time());
    return response;
}
NPPS_ENDPOINT("handover", "reserveTransfer", User, handover_reservetransfer, .batchable = false);

static void handover_aborttransfer(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    current_user->transfer_sha1.reset();
}
NPPS_ENDPOINT("handover", "abortTransfer", User, handover_aborttransfer, .batchable = false);

static KIDCheckResponse handover_kidcheck(Context &, const KIDCheckRequest &request) {
    // TODO
    util::log("handover.kidCheck stub: %s", request.to_json().dump().c_str());
    throw error::by_code(error::ERROR_KLAB_ID_SERVICE_NOT_REGISTERED);
}
NPPS_ENDPOINT("handover", "kidCheck", User, handover_kidcheck, .batchable = false);

static void handover_kidhandover(Context &) {
    // TODO
    util::log("handover.kidHandover stub");
    throw error::by_code(error::ERROR_KLAB_ID_SERVICE_NOT_REGISTERED);
}
NPPS_ENDPOINT("handover", "kidHandover", User, handover_kidhandover, .batchable = false);

static void handover_kidregister(Context &, const KIDCheckRequest &request) {
    // TODO
    util::log("handover.kidRegister stub: %s", request.to_json().dump().c_str());
    throw error::by_code(error::ERROR_KLAB_ID_SERVICE_ALREADY_REGISTERED);
}
NPPS_ENDPOINT("handover", "kidRegister", User, handover_kidregister, .batchable = false);

}  // namespace npps::game::handover
