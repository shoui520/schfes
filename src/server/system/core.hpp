#pragma once
// npps4/system/core.py
#include <cstdint>

namespace npps::system::core {

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Most of these information is obtained from https://w.atwiki.jp/lovelive-sif/pages/23.html unless noted //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

int64_t get_next_exp(int64_t rank);
// Cached (functools.cache).
int64_t get_next_exp_cumulative(int64_t rank);
int64_t get_invite_code(int64_t user_id);
int64_t get_energy_by_rank(int64_t rank);
int64_t get_max_friend_by_rank(int64_t rank);
int64_t get_training_energy_by_rank(int64_t rank);

}  // namespace npps::system::core
