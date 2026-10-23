// npps4/game/personalnotice.py
#include "app/idol.hpp"

namespace npps::game::personalnotice {

static PersonalNoticeGetResponse personalnotice_get(Context &context) {
    // https://github.com/DarkEnergyProcessor/NPPS/blob/v3.1.x/modules/personalnotice/get.php
    // TODO
    util::log("stub: personalnotice.get %s", context.raw_request_data.c_str());
    return PersonalNoticeGetResponse::make(false, 0, 0, "", "");
}
NPPS_ENDPOINT("personalnotice", "get", User, personalnotice_get);

}  // namespace npps::game::personalnotice
