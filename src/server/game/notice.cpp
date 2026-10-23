// npps4/game/notice.py
#include "app/idol.hpp"

namespace npps::game::notice {

static NoticeMarqueeResponse notice_noticemarquee(Context &context) {
    // TODO
    util::log("stub: notice.noticeMarquee %s", context.raw_request_data.c_str());
    return NoticeMarqueeResponse::make(0, {});
}
NPPS_ENDPOINT("notice", "noticeMarquee", User, notice_noticemarquee);

static NoticeFriendVarietyResponse notice_noticefriendvariety(Context &, const NoticeFriendVarietyRequest &request) {
    // TODO
    util::log("stub: notice.noticeFriendVariety filter_id=%lld readed=%d page=%lld", (long long)request.filter_id,
              (int)request.readed, (long long)request.page);
    return NoticeFriendVarietyResponse::make(0, {});
}
NPPS_ENDPOINT("notice", "noticeFriendVariety", User, notice_noticefriendvariety);

}  // namespace npps::game::notice
