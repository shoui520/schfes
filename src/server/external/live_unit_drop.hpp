#pragma once
// external/live_unit_drop.py: Live Show! unit reward drop (weights configurable in server_data.json).
#include "app/context.hpp"

namespace npps::external::live_unit_drop {

// unit_id to give to the player.
int64_t get_live_drop_unit(int64_t live_setting_id, Context &context);

}  // namespace npps::external::live_unit_drop
