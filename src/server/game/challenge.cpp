// npps4/game/challenge.py
#include "app/idol.hpp"

namespace npps::game::challenge {

static ChallengeInfoResponse challenge_challengeinfo(Context &context) {
    // TODO
    util::log("STUB challenge/challengeInfo %s", context.raw_request_data.c_str());
    return Json::array();  // idol.core.DummyModel()
}
NPPS_ENDPOINT("challenge", "challengeInfo", User, challenge_challengeinfo);

}  // namespace npps::game::challenge
