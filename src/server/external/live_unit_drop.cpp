// external/live_unit_drop.py
#include "external/live_unit_drop.hpp"

#include "data/data.hpp"

#include <map>

namespace npps::external::live_unit_drop {

static const data::ServerData *last_server_data = nullptr;
static std::map<int64_t, std::vector<int64_t>> cached_rare_drops;
static std::vector<int64_t> cached_common_drops;

static void extend_weighted(std::vector<int64_t> &out, int64_t unit_id, int64_t weight) {
    for (int64_t i = 0; i < weight; i++) out.push_back(unit_id);
}

static const data::ServerData &rebuild_cache() {
    // data::get() keeps old versions alive, so a different address means the data was reloaded.
    const data::ServerData &server_data = data::get();

    if (&server_data != last_server_data) {
        // Rebuild common drop cache
        cached_common_drops.clear();
        for (const auto &unit_drop : server_data.common_live_unit_drops)
            extend_weighted(cached_common_drops, unit_drop.unit_id, unit_drop.weight);

        // Rebuild live specific drop cache
        cached_rare_drops.clear();
        for (const auto &[lsid, live_unit_drops] : server_data.live_specific_live_unit_drops) {
            std::vector<int64_t> by_lsid;
            for (const auto &unit_drop : live_unit_drops) extend_weighted(by_lsid, unit_drop.unit_id, unit_drop.weight);
            cached_rare_drops[lsid] = std::move(by_lsid);
        }

        last_server_data = &server_data;
    }

    return server_data;
}

static int64_t get_drop_r(int64_t live_setting_id) {
    auto it = cached_rare_drops.find(live_setting_id);
    if (it != cached_rare_drops.end() && !it->second.empty()) return util::rng().choice(it->second);
    return 0;
}

static int64_t get_drop_n() { return util::rng().choice(cached_common_drops); }

int64_t get_live_drop_unit(int64_t live_setting_id, Context &) {
    const data::ServerData &server_data = rebuild_cache();
    int64_t unit_id = 0;

    // [_get_drop_n] * common + [_get_drop_r] * live_specific
    std::vector<int> choice_func;
    for (int64_t i = 0; i < server_data.live_unit_drop_chance.common; i++) choice_func.push_back(0);
    for (int64_t i = 0; i < server_data.live_unit_drop_chance.live_specific; i++) choice_func.push_back(1);

    while (unit_id == 0) unit_id = util::rng().choice(choice_func) == 0 ? get_drop_n() : get_drop_r(live_setting_id);

    return unit_id;
}

}  // namespace npps::external::live_unit_drop
