// npps4/game/event.py
#include "app/idol.hpp"

namespace npps::game::event {

static EventListResponse event_eventlist(Context &) {
    util::log("STUB event/eventList");
    throw error::by_code(error::ERROR_CODE_EVENT_NO_EVENT_DATA);
    // https://github.com/YumeMichi/honoka-chan/blob/6778972c1ff54a8a038ea07b676e6acdbb211f96/handler/event.go#L15
    // return EventListResponse(target_list=[EventTargetList(position=i, is_displayable=False) for i in range(1, 7)])
}
NPPS_ENDPOINT("event", "eventList", User, event_eventlist);

}  // namespace npps::game::event
