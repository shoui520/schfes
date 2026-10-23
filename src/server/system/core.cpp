// npps4/system/core.py
#include "system/core.hpp"

#include "core/util.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace npps::system::core {

static int64_t get_next_exp_base(int64_t rank) {
    if (rank <= 1) return 11;
    if (rank < 34) {
        // Rank < 34 is exactly this formula
        return util::pyround((double)get_next_exp_base(rank - 1) + 34.45 * (double)rank / 33);
    }
    return util::pyround(34.45 * (double)rank - 551);
}

int64_t get_next_exp(int64_t rank) {
    int64_t result = 0;
    if (rank > 0) {
        result = get_next_exp_base(rank);
        if (rank < 100) result = util::pyround((double)result / 2);
    }
    return result;
}

int64_t get_next_exp_cumulative(int64_t rank) {
    static std::map<int64_t, int64_t> cache;
    auto it = cache.find(rank);
    if (it != cache.end()) return it->second;
    int64_t sum = 0;
    for (int64_t i = 1; i <= rank; i++) sum += get_next_exp(i);
    cache.emplace(rank, sum);
    return sum;
}

int64_t get_invite_code(int64_t user_id) {
    // https://auahdark687291.blogspot.com/2018/05/sif-user-collision.html
    return util::pymod(user_id * 805306357, 999999937);
}

int64_t get_energy_by_rank(int64_t rank) {
    int64_t lp1 = util::floordiv(std::min<int64_t>(300, rank), 2) + 25;
    int64_t lp2 = util::floordiv(std::max<int64_t>(rank - 300, 0), 3);
    return lp1 + lp2;
}

int64_t get_max_friend_by_rank(int64_t rank) {
    int64_t friend1 = 10 + util::floordiv(std::min<int64_t>(50, rank), 5);
    int64_t friend2 = util::floordiv(std::max<int64_t>(rank - 50, 0), 10);
    return std::min<int64_t>(friend1 + friend2, 99);
}

int64_t get_training_energy_by_rank(int64_t rank) {
    int64_t m1 = 3 + util::floordiv(std::min<int64_t>(200, rank), 50);
    int64_t m2 = util::floordiv(std::max<int64_t>(rank - 200, 0), 100);
    return std::min<int64_t>(m1 + m2, 10);
}

}  // namespace npps::system::core
