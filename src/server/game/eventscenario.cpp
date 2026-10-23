// npps4/game/eventscenario.py
#include "app/idol.hpp"

namespace npps::game::eventscenario {

static EventScenarioStatusResponse eventscenario_status(Context &context) {
    // TODO
    util::log("stub: eventscenario.status %s", context.raw_request_data.c_str());
    return EventScenarioStatusResponse::make({});
}
NPPS_ENDPOINT("eventscenario", "status", User, eventscenario_status);

}  // namespace npps::game::eventscenario
