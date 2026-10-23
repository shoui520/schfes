#pragma once
// npps4/system/museum.py. MuseumParameterData, MuseumInfoData and MuseumMixin are generated in app/models.hpp.
#include "app/context.hpp"
#include "app/models.hpp"

namespace npps::system::museum {

inline constexpr bool TEST_MUSEUM_UNLOCK_ALL = false;

// Throws std::invalid_argument for an invalid museum contents id.
bool unlock(Context &context, db::main::User &user, int64_t museum_contents_id);
bool has(Context &context, db::main::User &user, int64_t museum_contents_id);
MuseumInfoData get_museum_info_data(Context &context, db::main::User &user);

}  // namespace npps::system::museum
