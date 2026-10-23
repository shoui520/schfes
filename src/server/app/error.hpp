#pragma once
// Error codes from npps4/idol/error.py.
#include <optional>
#include <stdexcept>
#include <string>

namespace npps::error {

constexpr int ERROR_CODE_NO_ERROR = 0;
constexpr int ERROR_CODE_LIB_ERROR = 1;
constexpr int ERROR_CODE_NETWORK_ERROR = 501;
constexpr int ERROR_CODE_LOGIN_FAILED = 502;
constexpr int ERROR_CODE_UNAVAILABLE = 503;
constexpr int ERROR_CODE_TIMEOUT = 504;
constexpr int ERROR_CODE_LOCKED = 423;
constexpr int ERROR_CODE_GAME_LOGIC_ERROR = 1001;
constexpr int ERROR_CODE_DUPLICATE_USER_NAME = 1100;
// Unable to register because symbols are included.
constexpr int ERROR_CODE_UNAVAILABLE_WORDS = 1101;
constexpr int ERROR_CODE_ENERGY_FULL = 1102;
constexpr int ERROR_CODE_NOT_ENOUGH_LOVECA = 1103;
constexpr int ERROR_CODE_NOT_ENOUGH_GAME_COIN = 1104;
// Inappropriate words contained.
constexpr int ERROR_CODE_NG_WORDS = 1105;
// Name could not be changed
constexpr int ERROR_CODE_ONLY_WHITESPACE_CHARACTERS = 1106;
constexpr int ERROR_CODE_OVER_UNIT_MAX = 1108;
constexpr int ERROR_CODE_OVER_FRIEND_MAX = 1109;
constexpr int ERROR_CODE_CHARACTER_COUNT_IS_OVER = 1112;
constexpr int ERROR_CODE_OVER_ADD_WAITING_UNIT_MAX_LIMIT = 1114;
constexpr int ERROR_CODE_NO_RECOVER_ITEM = 1130;
constexpr int ERROR_CODE_RECOVER_ITEM_NOT_ENOUGH = 1131;
constexpr int ERROR_CODE_ITEM_EXPIRED = 1140;
constexpr int ERROR_CODE_INCENTIVE_NONE = 1201;
constexpr int ERROR_CODE_OPEN_OTHERS = 1202;
constexpr int ERROR_CODE_LIMIT_OVER = 1203;
constexpr int ERROR_CODE_NOT_RELEASE_LOTTERY_TICKET = 1204;
constexpr int ERROR_CODE_NO_UNIT_IS_SELLABLE = 1205;
constexpr int ERROR_CODE_DUPLICATE_UNIT = 1301;
constexpr int ERROR_CODE_NO_CENTER_UNIT = 1302;
constexpr int ERROR_CODE_POSITION_EMPTY = 1303;
constexpr int ERROR_CODE_IGNORE_MAIN_DECK = 1304;
constexpr int ERROR_CODE_IGNORE_MAIN_DECK_UNIT = 1305;
constexpr int ERROR_CODE_NO_GAME_COIN = 1306;
constexpr int ERROR_CODE_UNIT_NOT_EXIST = 1311;
constexpr int ERROR_CODE_UNIT_NOT_DISPOSE = 1312;
constexpr int ERROR_CODE_UNIT_LEVEL_AND_SKILL_LEVEL_MAX = 1313;
constexpr int ERROR_CODE_IGNORE_FAVORITE_FLAG = 1321;
constexpr int ERROR_CODE_ACCESSORY_LOTTERY_LIST_HAS_DIFFERENCE = 1353;
constexpr int ERROR_CODE_BATTLE_ROOM_IS_INVALID = 1400;
constexpr int ERROR_CODE_BATTLE_DIFFICULTY_NOT_FOUND = 1401;
constexpr int ERROR_CODE_BATTLE_NPC_NOT_MATCHED = 1402;
constexpr int ERROR_CODE_BATTLE_NOT_EXIST = 1403;
constexpr int ERROR_CODE_EVENT_BATTLE_LIVE_NOT_FOUND = 1404;
constexpr int ERROR_CODE_EVENT_BATTLE_NEED_END_WAIT = 1405;
constexpr int ERROR_CODE_SECRET_BOX_NOT_EXIST = 1500;
constexpr int ERROR_CODE_SECRET_BOX_COST_TYPE_IS_NOT_SPECIFIED = 1501;
constexpr int ERROR_CODE_SECRET_BOX_INVALID_COST_TYPE = 1502;
constexpr int ERROR_CODE_SECRET_BOX_NOT_MULTI = 1503;
constexpr int ERROR_CODE_SECRET_BOX_AUTHORITY_ERROR = 1504;
constexpr int ERROR_CODE_SECRET_BOX_GAUGE_INFORMATION_NOT_EXIST = 1505;
constexpr int ERROR_CODE_SECRET_BOX_GAUGE_IS_FULL = 1506;
constexpr int ERROR_CODE_SECRET_BOX_REMAINING_COST_IS_NOT_ENOUGH = 1507;
constexpr int ERROR_CODE_SECRET_BOX_OUT_OF_DATE = 1508;
constexpr int ERROR_CODE_SECRET_BOX_UPPER_LIMIT = 1509;
constexpr int ERROR_CODE_SECRET_BOX_INVALID_TERM_COUNT = 1510;
constexpr int ERROR_CODE_SECRET_BOX_OUT_OF_DATE_PICK_UP_SCHEDULE = 1511;
constexpr int ERROR_CODE_SECRET_BOX_OUT_OF_STAMP_TERM = 1512;
constexpr int ERROR_CODE_SECRET_BOX_NO_AVAILABLE_SUBSCRIPTION = 1513;
constexpr int ERROR_CODE_USER_NOT_EXIST = 1601;
// Ranking data hasn't yet been calculated.
constexpr int ERROR_CODE_OUT_OF_RANG = 1602;
constexpr int ERROR_CODE_INVISIBLE_LIVE_INFO = 1603;
constexpr int INITIAL_DECK_NOT_EXISTS = 1701;
constexpr int INITIAL_MAIN_DECK_ALREADY_EXIST = 1702;
constexpr int ERROR_ALLIANCE_DUMMY = 1900;
constexpr int ERROR_ALLIANCE_DUMMY2 = 1901;
constexpr int ERROR_ALLIANCE_DUMMY3 = 1902;
constexpr int ERROR_CODE_SCENARIO_NOT_FOUND = 2300;
constexpr int ERROR_CODE_SCENARIO_NOT_AVAILABLE = 2301;
constexpr int ERROR_CODE_SCENARIO_LIVE_IS_PLAYING = 2302;
constexpr int ERROR_CODE_SCENARIO_NO_REWARD = 2303;
constexpr int ERROR_CODE_SCENARIO_IS_CLOSED = 2304;
constexpr int ERROR_CODE_SCENARIO_HAS_GOAL = 2305;
constexpr int ERROR_CODE_SUBSCENARIO_NOT_FOUND = 2350;
constexpr int ERROR_CODE_SUBSCENARIO_NOT_AVAILABLE = 2351;
constexpr int ERROR_CODE_SUBSCENARIO_LIVE_IS_PLAYING = 2352;
constexpr int ERROR_CODE_SUBSCENARIO_NO_REWARD = 2353;
constexpr int ERROR_CODE_SUBSCENARIO_IS_CLOSED = 2354;
constexpr int ERROR_CODE_EVENT_SCENARIO_HAS_ALREADY_OPENED = 2385;
constexpr int ERROR_CODE_EVENT_LACK_OF_COST = 2386;
// The Academy Student you specified doesn't exist.
constexpr int ERROR_CODE_FRIEND_USER_NOT_EXISTS = 2400;
constexpr int ERROR_CODE_FRIEND_SPECIFIED_USER_IS_NOT_A_FRIEND = 2401;
constexpr int ERROR_CODE_FRIEND_SPECIFIED_USER_IS_ARLEADY_FRIEND = 2402;
constexpr int ERROR_CODE_FRIEND_NOT_REQUESTED_FROM_SPECIFIED_USER = 2403;
constexpr int ERROR_CODE_FRIEND_NOT_REQUESTING_TO_SPECIFIED_USER = 2404;
constexpr int ERROR_CODE_FRIEND_COUNT_OVER_LIMIT = 2405;
constexpr int ERROR_CODE_FRIEND_ALREADY_REQUESTING_TO_SPECIFIED_USER = 2406;
constexpr int ERROR_CODE_FRIEND_ALREADY_REJECTED_FROM_SPECIFIED_USER = 2407;
constexpr int ERROR_CODE_FRIEND_ALREADY_CANCELED_FROM_SPECIFIED_USER = 2408;
constexpr int ERROR_CODE_FRIEND_COUNT_OVER_LIMIT_OF_APPLICANT = 2409;
constexpr int ERROR_CODE_FRIEND_OVER_REQUEST_COUNT = 2410;
constexpr int ERROR_CODE_FRIEND_OVER_PENDING_APPROVAL_COUNT = 2411;
constexpr int ERROR_CODE_GREET_INVALID_CHARACTOR = 2500;
constexpr int ERROR_CODE_ALBUM_REWARD_NONE = 2700;
constexpr int ERROR_CODE_ALBUM_REWARD_OPENED = 2701;
constexpr int ERROR_CODE_TUTORIAL_MAKE_FIRST_DECK_ERROR = 3000;
constexpr int ERROR_CODE_TUTORIAL_OPEN_FIRST_SCENARIO_ERROR = 3001;
constexpr int ERROR_CODE_TUTORIAL_SET_USER_SETTING_ERROR = 3002;
constexpr int ERROR_CODE_TUTORIAL_OPEN_FIRST_LIVE_ERROR = 3003;
constexpr int ERROR_CODE_TUTORIAL_BAD_STATE_ERROR = 3004;
constexpr int ERROR_CODE_TUTORIAL_IS_END = 3005;
constexpr int ERROR_CODE_SNS_COIN_LIMIT_OVER = 3100;
constexpr int ERROR_CODE_PAYMENT_INVALID_APPLE_PRODUCT_ID = 3101;
// The Live Show you specified is not available.
constexpr int ERROR_CODE_LIVE_NOT_FOUND = 3400;
// Required LP exceeds max LP
constexpr int ERROR_CODE_LIVE_NOT_ENOUGH_MAX_ENERGY = 3401;
// You don't have enough LP.
constexpr int ERROR_CODE_LIVE_NOT_ENOUGH_CURRENT_ENERGY = 3402;
constexpr int ERROR_CODE_LIVE_NOT_ENOUGH_LOVECA = 3403;
constexpr int ERROR_CODE_LIVE_INVALID_PARTY_USER = 3404;
constexpr int ERROR_CODE_LIVE_INVALID_UNIT_DECK = 3405;
constexpr int ERROR_CODE_LIVE_UNIT_LIMIT_OVER = 3406;
constexpr int ERROR_CODE_LIVE_NOTES_LEVEL_RATE_NOT_FOUND = 3407;
constexpr int ERROR_CODE_LIVE_NOTES_EFFECT_NOT_FOUND = 3408;
constexpr int ERROR_CODE_LIVE_UNIT_REWARD_NOT_FOUND = 3409;
constexpr int ERROR_CODE_LIVE_PARTY_MEMBER_INFO_NOT_FOUND = 3410;
constexpr int ERROR_CODE_LIVE_PLAY_DATA_NOT_FOUND = 3411;
// You don't have enough <token name>.
constexpr int ERROR_CODE_LIVE_NOT_ENOUGH_EVENT_POINT = 3412;
constexpr int ERROR_CODE_LIVE_NOT_MATCH_LIVE_RANK = 3413;
// An error has occured. Please reboot.
constexpr int ERROR_CODE_LIVE_UNKNOWN_ERROR = 3414;
constexpr int ERROR_CODE_LIVE_LEADER_SKILL_NOT_FOUND = 3415;
constexpr int ERROR_CODE_LIVE_NOTES_LIST_NOT_FOUND = 3416;
// The Live Show you specified is not available.
constexpr int ERROR_CODE_LIVE_NOT_OPENED = 3417;
// This event is not in progress.
constexpr int ERROR_CODE_LIVE_EVENT_HAS_GONE = 3418;
constexpr int ERROR_CODE_LIVE_UNEXPECTED_END = 3419;
// No record is available
constexpr int ERROR_CODE_LIVE_PRECISE_LIST_NOT_FOUND = 3421;
constexpr int ERROR_CODE_LIVE_RETRY = 3422;
// Use <amount of money> G to recover one past Master song play?
constexpr int ERROR_CODE_LIVE_NOT_ENOUGH_TRAINING_ENERGY = 3423;
// There are no past Master song plays remaining.
// The amount will reset at midnight JST.
constexpr int ERROR_CODE_LIVE_NOT_ENOUGH_TRAINING_ENERGY_CANT_RECOVERY = 3424;
constexpr int ERROR_CODE_LIVE_MATCHING_NO_ROOM = 3450;
constexpr int ERROR_CODE_LIVE_MATCHING_ROOM_FULL = 3451;
constexpr int ERROR_CODE_LIVE_MATCHING_USER_NOT_ENOUGH = 3452;
constexpr int ERROR_CODE_LIVE_MATCHING_CANNOT_LEAVE = 3453;
constexpr int ERROR_CODE_LIVE_MATCHING_INVALID_TERM = 3454;
constexpr int ERROR_CODE_LIVE_GPS_MATCHING_NOT_OPENED = 3455;
constexpr int ERROR_CODE_LIVE_MATCHING_STATUS_UPDATED = 3456;
constexpr int ERROR_CODE_LIVE_MATCHING_DISMISS = 3457;
constexpr int ERROR_CODE_LIVE_POLLING_DISMISS = 3459;
constexpr int ERROR_CODE_LIVE_MATCHING_CANNOT_OPEN = 3460;
constexpr int ERROR_CODE_LIVE_ARENA_MATCH_NOT_AUTO_PLAYABLE = 3470;
constexpr int ERROR_CODE_LIVE_ARENA_MATCH_AUTO_PLAYABLE = 3471;
constexpr int ERROR_CODE_LIVE_ARENA_MATCH_NO_LIVE_PRECISE_LOG = 3472;
constexpr int ERROR_CODE_LIVE_ARENA_MATCH_LIVE_PRECISE_LOG_UPDATED = 3473;
constexpr int ERROR_CODE_LIVE_ARENA_MATCH_USER_NOT_FOUND = 3474;
constexpr int ERROR_CODE_LIVE_ARENA_MATCH_USER_RATING_UPDATED = 3475;
constexpr int ERROR_CODE_LIVE_ARENA_DREAM_SESSION_EXPIRED = 3480;
constexpr int ERROR_CODE_LIVE_ARENA_DREAM_SESSION_NOT_MEET_PLAY_REQUIREMENTS = 3481;
// Exchange limit reached.
constexpr int ERROR_CODE_OVER_ADD_EXCHANGE_ITEM_COUNT_MAX_LIMIT = 4201;
// You do not have enough <exchange currency>.
constexpr int ERROR_CODE_NOT_ENOUGH_EXCHANGE_POINT = 4202;
// This item has expired.
constexpr int ERROR_CODE_EXCHANGE_ITEM_OUT_OF_DATE = 4203;
// Sticker shop is currently unavailable! Please try again later.
constexpr int ERROR_CODE_EXCHANGE_INVALID = 4204;
// No longer valid.
constexpr int ERROR_CODE_EXCHANGE_POINT_OUT_OF_DATE = 4215;
constexpr int ERROR_CODE_NOT_ACTIVE_DISCOUNT_LICENSE = 4216;
constexpr int ERROR_CODE_AWARD_ALREADY_GAINED = 4205;
constexpr int ERROR_CODE_BACKGROUND_ALREADY_GAINED = 4206;
constexpr int ERROR_CODE_AWARD_IS_CONTAINED_IN_BOX = 4207;
constexpr int ERROR_CODE_BACKGROUND_IS_CONTAINED_IN_BOX = 4208;

// This Transfer Passcode is expired.
constexpr int ERROR_HANDOVER_EXPIRE = 4401;
// This Transfer Passcode is incorrect.
constexpr int ERROR_HANDOVER_NONE = 4402;
// You can't transfer data within the same device, nor if you still have purchased items such as :Loveca:Love Gems.
constexpr int ERROR_HANDOVER_SELF = 4403;
// This passcode has already been used.
constexpr int ERROR_HANDOVER_DISABLE_MU = 4404;
// Game data can't be transferred from this account.
// Please wait for a while and try again.
constexpr int ERROR_HANDOVER_LOCKED_USER = 4405;
// Game data can't be transferred to this account.
// Please wait for a while and try again.
constexpr int ERROR_HANDOVER_LOCKED_TARGET = 4406;
// Invaid Transfer ID or Transfer Passcode.
// Please try again.
constexpr int ERROR_HANDOVER_INVALID_ID_OR_CODE = 4407;
// You can't transfer your game data to a different OS as you still have Love Gems and/or purchased Scouting Tickets.
constexpr int ERROR_HANDOVER_PAYMENT_ITEM_POSSESSION = 4408;

// SIF ID cannot be accessed at this time.
constexpr int ERROR_KLAB_ID_SERVICE_NOT_AVAILABLE = 4490;
// Please log in again.
constexpr int ERROR_KLAB_ID_SERVICE_LOGIN_AGAIN = 4491;
// SIF ID is currently under maintenance.
constexpr int ERROR_KLAB_ID_SERVICE_MAINTENANCE = 4492;
// There is no Love Live! School Idol Festival game data registered to this SIF ID account.
constexpr int ERROR_KLAB_ID_SERVICE_NOT_REGISTERED = 4493;
// There is a Love Live! School Idol Festival game data registered to this SIF ID account.
// Registered game data can be removed from Menu > Settings.
constexpr int ERROR_KLAB_ID_SERVICE_ALREADY_REGISTERED = 4494;
// A data mismatch was found.
// You may be logged in to the incorrect SIF ID.
constexpr int ERROR_KLAB_ID_SERVICE_NOT_MATCH = 4495;
// An unexpected error has occured.
constexpr int ERROR_KLAB_ID_SERVICE_NOT_HAS_ENOUGH_SCOPE = 4496;
// An unexpected error has occured.
constexpr int ERROR_KLAB_ID_SERVICE_TASK_DISABLED = 8001;

constexpr int ERROR_CODE_ONLINE_PLAY_COUNT_LIMIT_OVER = 4701;
constexpr int ERROR_CODE_ONLINE_LIVE_HAS_GONE = 4702;
constexpr int ERROR_CODE_FINAL_VOTE_OUT_TERM = 4710;
constexpr int ERROR_CODE_FINAL_NO_RESULT = 4720;
constexpr int ERROR_CODE_FINAL_MAINTENANCE = 4740;
constexpr int ERROR_DOWNLOAD_NO_ADDITIONAL_PACKAGE = 4501;
constexpr int ERROR_DOWNLOAD_NO_UPDATE_PACKAGE = 4502;
constexpr int ERROR_CODE_EVENT_REWARD_OPENED = 10000;
constexpr int ERROR_CODE_EVENT_REWARD_NOT_GETTABLE = 10001;
constexpr int ERROR_CODE_EVENT_NO_EVENT_POINT_COUNT_DATA = 10002;
constexpr int ERROR_CODE_EVENT_NO_EVENT_POINT_USER_DATA = 10003;
constexpr int ERROR_CODE_EVENT_NO_EVENT_DATA = 10004;
constexpr int ERROR_CODE_LL_ACCOUNT_ALREADY_CONNECTED = 6000;
constexpr int ERROR_CODE_PF_ACCOUNT_ALREADY_CONNECTED = 6001;
constexpr int ERROR_CODE_CONNECTION_DOSE_NOT_EXISTS = 6002;
constexpr int ERROR_CODE_INVALID_USERS_HANDOVER = 6003;
constexpr int ERROR_CODE_INVALID_PLATFORM_HANDOVER = 6004;
constexpr int ERROR_CODE_STAMP_TYPE_UNDEFINED = 6100;
constexpr int ERROR_CODE_STAMP_SETTING_ID_INVALID = 6101;
constexpr int ERROR_CODE_STAMP_COUNT = 6103;
constexpr int ERROR_CODE_STAMP_TYPE_MISS_MATCH = 6104;
constexpr int ERROR_CODE_STAMP_DUPLICATE_POSITION = 6105;
constexpr int ERROR_CODE_STAMP_DUPLICATE_STAMP = 6106;
constexpr int ERROR_CODE_STAMP_NOT_OWNED = 6107;
constexpr int ERROR_CODE_STAMP_NO_MAIN = 6108;
constexpr int ERROR_CODE_AC_MAINT = 11101;
constexpr int ERROR_CODE_AC_NO_DATA = 11102;
constexpr int ERROR_CODE_AREA_ALREADY_RECEIVED = 4801;
constexpr int ERROR_CODE_AREA_OUT_OF_AREA = 4802;
constexpr int ERROR_CODE_AREA_OUT_OF_DATE = 4803;
constexpr int ERROR_CODE_AREA_EXCEEDED_POSSESSION_LIMIT = 4804;
constexpr int ERROR_CODE_SNS_ALREADY_RECEIVED = 5001;
constexpr int ERROR_CODE_SNS_OUT_OF_DATE = 5002;
constexpr int ERROR_CODE_SNS_ALREADY_HAS_NON_DUPLICABLE_ITEM = 5003;
constexpr int ERROR_CODE_FESTIVAL_MISSION_EXPIRED = 9902;
constexpr int ERROR_CODE_ITEM_USE_USED_ITEM = 21001;
constexpr int ERROR_CODE_ITEM_USE_OUT_OF_EVENT = 21002;
constexpr int ERROR_CODE_CLASS_CHALLENGE_LIVE_TERM_OVER = 7100;
constexpr int ERROR_CODE_NO_AD_REWARD_REST = 13001;
constexpr int ERROR_CODE_NO_SKIT_AVAILABLE = 14001;
constexpr int ERROR_CODE_RECEIPT_FAIL_ACKNOWLEDGE = 22001;
constexpr int ERROR_CODE_LOOT_BOX_EXPIRE = 23001;
constexpr int ERROR_CODE_DELETE_RESERVED_USER = 25001;
constexpr int ERROR_CODE_DELETED_USER = 25002;



// idol.error.IdolError: a game-level error sent as response_data.
struct IdolError : std::exception {
    int error_code, status_code;
    std::optional<std::string> detail;
    int http_code;
    explicit IdolError(int error_code = 200, int status_code = 720, std::optional<std::string> detail = std::nullopt,
                       int http_code = 200)
        : error_code(error_code), status_code(status_code), detail(std::move(detail)), http_code(http_code) {}
    const char *what() const noexcept override { return detail ? detail->c_str() : "IdolError"; }
};

inline IdolError by_code(int code) { return IdolError(code, 600); }
inline IdolError locked() { return IdolError(423, 600, "F", 423); }

// fastapi.HTTPException: plain {"detail": ...} response without signing.
struct HttpError : std::exception {
    int status;
    std::string detail;
    HttpError(int status, std::string detail) : status(status), detail(std::move(detail)) {}
    const char *what() const noexcept override { return detail.c_str(); }
};

// pydantic.ValidationError for request models.
struct ValidationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace npps::error
