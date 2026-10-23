// npps4/game/banner.py
#include "app/idol.hpp"
#include "system/user.hpp"

namespace npps::game::banner {

static BannerListResponse banner_bannerlist(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    // TODO
    util::log("STUB banner/bannerList %s", context.raw_request_data.c_str());

    // SIF2 transfer banner
    BannerInfo transfer = BannerInfo::make(
        18, 1,
        context.lang == Language::en ? "en/assets/image/handover/banner/banner_01.png"
                                     : "assets/image/handover/banner/banner_01.png",
        false, false, 1800002, util::timestamp_to_datetime(1476522000), util::timestamp_to_datetime(2147483647));
    transfer.is_registered = current_user->transfer_sha1.has_value();

    // TenFes banner
    BannerInfo tenfes = BannerInfo::make(
        2, 1,
        context.lang == Language::en ? "en/assets/image/webview/wv_ba_01.png" : "assets/image/webview/wv_ba_01.png",
        false, true, 200001, util::timestamp_to_datetime(1476522000), util::timestamp_to_datetime(2147483647));
    tenfes.webview_url = "/";

    return BannerListResponse::make(util::timestamp_to_datetime(2147483647), {transfer, tenfes});
}
NPPS_ENDPOINT("banner", "bannerList", User, banner_bannerlist, .exclude_none = true);

}  // namespace npps::game::banner
