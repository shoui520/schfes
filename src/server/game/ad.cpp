// npps4/game/ad.py
#include "app/idol.hpp"

namespace npps::game::ad {

static void ad_adchange(Context &context) {
    // TODO
    util::log("stub ad/changeAd: %s", context.raw_request_data.c_str());
}
NPPS_ENDPOINT("ad", "changeAd", User, ad_adchange);

}  // namespace npps::game::ad
