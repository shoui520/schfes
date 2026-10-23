#pragma once
// npps4/system/profile.py. ProfileUnitInfo is generated in app/models.hpp.
#include "app/context.hpp"
#include "app/models.hpp"

namespace npps::system::profile {

ProfileUnitInfo to_profile_unit_info(Context &context, const db::main::Unit &unit_data,
                                     const museum::MuseumParameterData &museum_param);

}  // namespace npps::system::profile
