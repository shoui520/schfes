// npps4/game/profile.py
#include "app/idol.hpp"
#include "system/advanced.hpp"
#include "system/album.hpp"
#include "system/live.hpp"
#include "system/museum.hpp"
#include "system/profile.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

namespace npps::game::profile {

static db::main::User &get_target_user(Context &context, int64_t user_id) {
    db::main::User *target_user = system::user::get(context, user_id);
    if (!target_user) throw error::by_code(error::ERROR_CODE_USER_NOT_EXIST);
    return *target_user;
}

static std::vector<ProfileLiveCount> profile_livecount(Context &context, const ProfileRequest &request) {
    db::main::User &target_user = get_target_user(context, request.user_id);
    std::map<int64_t, int64_t> cleared = system::live::get_cleared_live_count(context, target_user);
    std::vector<ProfileLiveCount> result;
    for (int64_t i : {1, 2, 3, 4, 6}) {
        auto it = cleared.find(i);
        result.push_back(ProfileLiveCount::make(i, it == cleared.end() ? 0 : it->second));
    }
    return result;
}
NPPS_ENDPOINT("profile", "liveCnt", User, profile_livecount);

static std::vector<ProfileCardRanking> profile_cardranking(Context &context, const ProfileRequest &request) {
    db::main::User &target_user = get_target_user(context, request.user_id);
    std::vector<ProfileCardRanking> result;
    for (const db::main::Album *album_info : system::album::all_ranking(context, target_user))
        result.push_back(ProfileCardRanking::make(album_info->unit_id, album_info->favorite_point,
                                                  album_info->rank_max_flag + 1, album_info->sign_flag));
    return result;
}
NPPS_ENDPOINT("profile", "cardRanking", User, profile_cardranking);

static ProfileInfoResponse profile_profileinfo(Context &context, const ProfileRequest &request) {
    db::main::User &target_user = get_target_user(context, request.user_id);

    int64_t partner_unit_owning_user_id = system::unit::get_unit_center(context, target_user);
    if (partner_unit_owning_user_id == 0) throw error::by_code(error::ERROR_CODE_USER_NOT_EXIST);
    db::main::Unit *partner_unit = system::unit::get_unit(context, partner_unit_owning_user_id);
    system::unit::validate_unit(target_user, partner_unit);

    auto active_deck = system::unit::load_unit_deck(context, target_user, target_user.active_deck_index);
    if (!active_deck) throw error::by_code(error::ERROR_CODE_USER_NOT_EXIST);

    db::main::Unit *center_unit = system::unit::get_unit(context, std::get<1>(*active_deck)[4]);

    int64_t unit_count = system::unit::count_units(context, target_user, true);
    system::museum::MuseumInfoData museum_data = system::museum::get_museum_info_data(context, target_user);

    ProfileUserInfo user_info;
    user_info.user_id = request.user_id;
    user_info.name = target_user.name;
    user_info.level = target_user.level;
    user_info.unit_max = target_user.unit_max;
    user_info.energy_max = target_user.energy_max;
    user_info.friend_max = target_user.friend_max;
    user_info.unit_cnt = unit_count;
    user_info.invite_code = target_user.invite_code;
    user_info.introduction = target_user.bio;

    ProfileInfoResponse response;
    response.user_info = std::move(user_info);
    response.center_unit_info = system::profile::to_profile_unit_info(context, *center_unit, museum_data.parameter);
    response.navi_unit_info = system::profile::to_profile_unit_info(context, *partner_unit, museum_data.parameter);
    response.is_alliance = false;  // TODO
    response.friend_status = 0;
    response.setting_award_id = target_user.active_award;
    response.setting_background_id = target_user.active_background;
    return response;
}
NPPS_ENDPOINT("profile", "profileInfo", User, profile_profileinfo);

static void profile_profileregister(Context &context, const ProfileRegisterRequest &request) {
    system::advanced::test_name(context, request.introduction);
    db::main::User *current_user = system::user::get_current(context);
    current_user->bio = request.introduction;
}
NPPS_ENDPOINT("profile", "profileRegister", User, profile_profileregister);

}  // namespace npps::game::profile
