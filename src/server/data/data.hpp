#pragma once
// npps4/data/__init__.py: server_data.json loader.
#include "app/models.hpp"
#include "data/schema.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace npps::data {

// Insertion-ordered dict (iteration order matters for secretbox pages and update()).
template <typename K, typename V> class OrderedMap {
public:
    using value_type = std::pair<K, V>;

    // dict[key] = value: replaces in place, keeps the original position.
    V &set(const K &key, V value) {
        auto it = index_.find(key);
        if (it != index_.end()) return items_[it->second].second = std::move(value);
        index_.emplace(key, items_.size());
        items_.emplace_back(key, std::move(value));
        return items_.back().second;
    }
    const V *find(const K &key) const {
        auto it = index_.find(key);
        return it == index_.end() ? nullptr : &items_[it->second].second;
    }
    bool contains(const K &key) const { return index_.count(key) != 0; }
    const V &at(const K &key) const {
        const V *v = find(key);
        if (!v) throw std::out_of_range("key not found");
        return *v;
    }
    size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }
    auto begin() const { return items_.begin(); }
    auto end() const { return items_.end(); }

private:
    std::vector<value_type> items_;
    std::unordered_map<K, size_t> index_;
};

struct ServerData {
    std::optional<std::string> json_schema_link;
    std::vector<std::string> badwords;  // base64url-decoded
    OrderedMap<int64_t, std::vector<model::Ptr<system::item_model::BaseItem>>> achievement_reward;
    schema::LiveUnitDropChance live_unit_drop_chance;
    std::vector<schema::LiveUnitDrop> common_live_unit_drops;
    OrderedMap<int64_t, std::vector<schema::LiveUnitDrop>> live_specific_live_unit_drops;
    OrderedMap<int64_t, std::vector<schema::BaseItemWithWeight>> live_effort_drops;
    OrderedMap<int64_t, schema::SecretboxData> secretbox_data;  // key: secretbox_id
    std::vector<schema::SerialCode> serial_codes;
    std::vector<schema::StickerShop> sticker_shop;
};

// Loads (or reloads when the file's mtime changed) config server_data.json.
// Old versions are kept alive, so the reference stays valid for the process
// lifetime. Throws if the first load fails; later failures keep the old data.
const ServerData &get();
// Writes the current data back to server_data.json (scripts/reformat_server_data.py).
void update();

}  // namespace npps::data
