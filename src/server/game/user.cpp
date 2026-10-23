// npps4/game/user.py
#include "app/idol.hpp"
#include "system/advanced.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

namespace npps::game::user {

static ChangeNameResponse user_changename(Context &context, const ChangeNameRequest &request) {
    system::advanced::test_name(context, request.name);
    db::main::User *current_user = system::user::get_current(context);
    std::string oldname = current_user->name;
    current_user->name = request.name;
    return ChangeNameResponse::make(oldname, request.name);
}
NPPS_ENDPOINT("user", "changeName", User, user_changename, .batchable = false);

static UserGetNaviResponse user_getnavi(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    int64_t center = system::unit::get_unit_center(context, *current_user);
    UserGetNaviResponse response;
    response.user = UserNavi::make(current_user->id, center);
    return response;
}
NPPS_ENDPOINT("user", "getNavi", User, user_getnavi);

static UserInfoResponse user_userinfo(Context &context) {
    db::main::User *u = system::user::get_current(context);
    if (u == nullptr) throw error::IdolError(error::ERROR_CODE_LIB_ERROR, 500, "User is not known", 500);
    UserInfoResponse response = UserInfoResponse::make(system::user::get_user_info(context, *u));
    response.ad_status = false;
    return response;
}
NPPS_ENDPOINT("user", "userInfo", User, user_userinfo, .exclude_none = true);

static void user_setnotificationtoken(Context &, const UserNotificationTokenRequest &) {
    // TODO
    util::log("stub: user.setNotificationToken");
}
NPPS_ENDPOINT("user", "setNotificationToken", User, user_setnotificationtoken, .batchable = false);

static void user_changenavi(Context &context, const UserChangeNaviRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    db::main::Unit *unit_data = system::unit::get_unit(context, request.unit_owning_user_id);
    system::unit::validate_unit(*current_user, unit_data);
    system::unit::set_unit_center(context, *current_user, unit_data);
}
NPPS_ENDPOINT("user", "changeNavi", User, user_changenavi);

static UserAddUnitResponse user_addunitmax(Context &context, const UserAddUnitMaxRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    int64_t current_loveca = system::user::get_loveca(*current_user);
    if (current_loveca < request.amount) throw error::IdolError(200, 720, "not enough loveca");

    // FIXME: Add limit
    system::user::sub_loveca(*current_user, request.amount);
    current_user->unit_max = current_user->unit_max + request.amount * 4;
    return UserAddUnitResponse::make(current_loveca - system::user::get_loveca(*current_user),
                                     current_user->unit_max);
}
NPPS_ENDPOINT("user", "addUnitMax", User, user_addunitmax);

// user_setnotification is not registered in NPPS4 either (the decorator is missing).

}  // namespace npps::game::user
