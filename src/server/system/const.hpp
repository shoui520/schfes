#pragma once
// npps4/const.py plain constants. The enums are generated in app/models.hpp
// (same namespace), which this header includes.
#include "app/models.hpp"

#include <array>
#include <cstdint>

namespace npps::const_ {

// https://w.atwiki.jp/lovelive-sif/pages/38.html
inline constexpr std::array<int64_t, 4> LIVE_EXP_DROP = {12, 26, 43, 83};
inline constexpr std::array<std::array<int64_t, 5>, 4> LIVE_GOLD_DROP = {{
    {2250, 1800, 1200, 600, 300},
    {3000, 2400, 1600, 800, 400},
    {3750, 3000, 2000, 1000, 500},
    {4500, 3600, 2400, 1200, 600},
}};

inline constexpr int64_t SUBSCENARIO_LOVECA_REWARD_AMOUNT = 1;
inline constexpr int64_t SUBSCENARIO_GAME_COIN_REWARD_AMOUNT = 20000;
inline constexpr const char *SUBSCENARIO_REWARD_COMMENT_EN = "Side story cleared!";
inline constexpr const char *SUBSCENARIO_REWARD_COMMENT_JP = "サイドストーリークリア！";

inline constexpr int64_t COMMON_UNIT_EXPIRY = 86400 * 60;  // 60 days
inline constexpr int64_t MAX_EFFORT_SCORE = 300000000;     // 300 million

}  // namespace npps::const_
