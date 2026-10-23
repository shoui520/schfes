// npps4/game/login.py
#include "app/idol.hpp"
#include "app/session.hpp"
#include "core/crypto.hpp"
#include "system/achievement.hpp"
#include "system/reward.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <array>

namespace npps::game::login {

namespace s_user = ::npps::system::user;
namespace s_unit = ::npps::system::unit;
namespace s_reward = ::npps::system::reward;
namespace s_achievement = ::npps::system::achievement;

// npps4/idol/cache.py clear()
static void cache_clear(Context &context, int64_t user_id) {
    context.db.main.execute("DELETE FROM request_cache WHERE user_id = ?", user_id);
}

static LoginResponse login_login(Context &context, const LoginRequest &request) {
    // Login user
    if (!context.token) throw std::logic_error("token is None");
    // Decrypt credentials
    Bytes key = util::xorbytes(context.token->client_key.substr(0, 16), context.token->server_key.substr(0, 16));
    Bytes loginkey = crypto::decrypt_aes(key, util::base64_decode(request.login_key));
    Bytes passwd = crypto::decrypt_aes(key, util::base64_decode(request.login_passwd));

    // Find user
    db::main::User *u = s_user::find_by_key(context, loginkey);
    if (!u || !s_user::check_passwd(*u, passwd)) {
        // This will send "Your data has been transfered succesfully" message to the SIF client.
        throw error::IdolError(407, 600, "Login not found");
    }

    // Login
    session::invalidate_current(context);
    if (u->locked) throw error::locked();

    std::string token = session::encapsulate_token(context, context.token->server_key, context.token->client_key, u->id);
    cache_clear(context, u->id);
    return LoginResponse::make(token, u->id);
}
NPPS_ENDPOINT("login", "login", Auth, login_login, .check_version = false, .batchable = false);

static AuthkeyResponse login_authkey(Context &context, const AuthkeyRequest &request) {
    // Generate authentication key.

    // Decrypt client key
    Bytes client_key = crypto::decrypt_rsa(util::base64_decode(request.dummy_token));
    if (client_key.empty()) throw error::HttpError(400, "Bad client key");
    Bytes auth_data = crypto::decrypt_aes(client_key.substr(0, 16), util::base64_decode(request.auth_data));
    (void)auth_data;

    // Create new token
    Bytes server_key = util::randbytes(32);
    std::string token = session::encapsulate_token(context, server_key, client_key);

    // Return response
    return AuthkeyResponse::make(token, util::base64_encode(server_key));
}
NPPS_ENDPOINT("login", "authkey", Params, login_authkey, .check_version = false, .batchable = false,
              .xmc_verify = XMCVerifyMode::NONE);

static StartupResponse login_startup(Context &context, const LoginRequest &request) {
    // Register new account.
    if (!context.token) throw std::logic_error("token is None");
    Bytes key = util::xorbytes(context.token->client_key.substr(0, 16), context.token->server_key.substr(0, 16));
    Bytes loginkey = crypto::decrypt_aes(key, util::base64_decode(request.login_key));
    Bytes passwd = crypto::decrypt_aes(key, util::base64_decode(request.login_passwd));

    // Create user
    db::main::User *u = s_user::create(context, loginkey, passwd);
    session::invalidate_current(context);
    return StartupResponse::make(std::to_string(u->id));
}
NPPS_ENDPOINT("login", "startUp", Auth, login_startup, .check_version = false, .batchable = false);

static TopInfoResponse login_topinfo(Context &context) {
    // TODO
    util::log("STUB login/topInfo %s", context.raw_request_data.c_str());
    db::main::User *current_user = s_user::get_current(context);
    return TopInfoResponse::make(
        0, 0, 0, 0, s_reward::count_presentbox(context, *current_user), false, util::timestamp_to_datetime(util::time()),
        util::timestamp_to_datetime(86400), util::timestamp_to_datetime(86400), 0, 0, false,
        LicenseInfo::make(Json::array(), Json::array(), Json::array(), false), Json::array(), false, false, false,
        {135, 41, 345}, false, false);
}
NPPS_ENDPOINT("login", "topInfo", User, login_topinfo);

static TopInfoOnceResponse login_topinfoonce(Context &context) {
    db::main::User *current_user = s_user::get_current(context);
    // TODO
    util::log("STUB login/topInfoOnce %s", context.raw_request_data.c_str());
    return TopInfoOnceResponse::make(
        0, s_achievement::get_achievement_count(context, *current_user, false), false, current_user->training_energy,
        current_user->training_energy_max,
        NotificationStatus::make(true, false, false, false, false, false, true, true, true), true, true, true, true, true);
}
NPPS_ENDPOINT("login", "topInfoOnce", User, login_topinfoonce);

static const std::array<int64_t, 9> TEMPLATE_DECK = {13, 9, 8, 23, 0, 24, 21, 20, 19};
static const std::array<std::array<int64_t, 9>, 2> INITIAL_UNIT_IDS = {{
    // Unfortunately the game doesn't preload the R card asset anymore.
    // so this is not configurable.
    // Myus
    {1131, 1789, 378, 1997, 2085, 703, 367, 330, 1912},  // UR
    // range(49, 58),  # R
    // Aqua
    {1308, 1813, 1298, 2158, 1743, 2200, 2310, 2236, 1372},  // UR
    // range(788, 797),  # R
}};

static std::vector<int64_t> _generate_deck_list(int64_t unit_id) {
    std::vector<int64_t> template_copy(TEMPLATE_DECK.begin(), TEMPLATE_DECK.end());
    template_copy[4] = unit_id;
    return template_copy;
}

static StarterUnitListResponse login_unitlist(Context &) {
    StarterUnitListResponse response;
    int64_t catid = 1;
    for (const auto &unit_list : INITIAL_UNIT_IDS) {
        std::vector<StarterUnitInitialSetInfo> sets;
        int64_t initial_id = 1 + (catid - 1) * 9;
        for (int64_t center_uid : unit_list) {
            std::vector<StarterUnitListInfo> units;
            for (int64_t uid : _generate_deck_list(center_uid)) units.push_back(StarterUnitListInfo::make(uid, uid == center_uid));
            sets.push_back(StarterUnitInitialSetInfo::make(initial_id, std::move(units), center_uid));
            initial_id++;
        }
        response.member_category_list.push_back(StarterMemberCategory::make(catid, std::move(sets)));
        catid++;
    }
    return response;
}
NPPS_ENDPOINT("login", "unitList", User, login_unitlist);

static StarterUnitSelectResponse login_unitselect(Context &context, const StarterUnitSelectRequest &request) {
    if (request.unit_initial_set_id < 1 || request.unit_initial_set_id >= 19) throw error::IdolError(200, 720, "Out of range");

    db::main::User *current_user = s_user::get_current(context);
    if (current_user->tutorial_state != 1) throw error::IdolError(200, 720, "Invalid tutorial state");

    int64_t target = request.unit_initial_set_id - 1;
    std::vector<int64_t> unit_ids = _generate_deck_list(INITIAL_UNIT_IDS[target / 9][target % 9]);

    std::vector<db::main::Unit *> units;
    for (int64_t uid : unit_ids) {
        db::main::Unit *unit_object = s_unit::add_unit_simple(context, *current_user, uid, true);
        if (!unit_object) throw std::runtime_error("unable to add units");

        units.push_back(unit_object);
    }

    // Idolize center
    db::main::Unit *center = units[4];
    s_unit::idolize(context, *current_user, *center);
    s_unit::set_unit_center(context, *current_user, center);

    auto deck = s_unit::load_unit_deck(context, *current_user, 1, true);
    if (!deck) throw std::logic_error("deck is None");
    std::vector<int64_t> ids;
    for (auto *u : units) ids.push_back(u->id);
    s_unit::save_unit_deck(context, *current_user, *std::get<0>(*deck), ids);
    return StarterUnitSelectResponse::make(unit_ids);
}
NPPS_ENDPOINT("login", "unitSelect", User, login_unitselect);

}  // namespace npps::game::login
