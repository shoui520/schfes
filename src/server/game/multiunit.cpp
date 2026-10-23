// npps4/game/multiunit.py
#include "app/idol.hpp"

namespace npps::game::multiunit {

static MultiUnitScenarioResponse multiunit_multiunitscenariostatus(Context &context) {
    // TODO
    util::log("stub: multiunit.multiunitscenarioStatus %s", context.raw_request_data.c_str());
    return MultiUnitScenarioResponse::make({});
}
NPPS_ENDPOINT("multiunit", "multiunitscenarioStatus", User, multiunit_multiunitscenariostatus);

}  // namespace npps::game::multiunit
