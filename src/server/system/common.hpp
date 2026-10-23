#pragma once
// npps4/system/common.py. The models (BeforeAfter, BaseRewardInfo, ItemCount,
// TimestampMixin, CenterUnitInfo) are generated in app/models.hpp.
#include "app/context.hpp"
#include "app/models.hpp"

#include <map>
#include <type_traits>

namespace npps::system::common {

// unit_model.AnyUnitItem | scenario_model.ScenarioItem | live_model.LiveItem | item_model.Item:
// every member derives from Item, so the union is a polymorphic Item pointer.
using AnyItem = model::Ptr<item_model::Item>;

// get_cached / @context_cacheable(key): per-request memo keyed by `id`.
// As in Python, a None (null pointer) result is not remembered. All callers of
// one key must use the same T and U.
template <typename U, typename T, typename F> U get_cached(Context &context, const char *key, const T &id, F &&miss) {
    auto &cache = context.memo<std::map<T, U>>(key);
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;
    U result = miss(context, id);
    if constexpr (std::is_pointer_v<U>) {
        if (result == nullptr) return result;
    }
    cache.emplace(id, result);
    return result;
}

}  // namespace npps::system::common
