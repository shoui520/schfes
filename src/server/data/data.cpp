// npps4/data/__init__.py
#include "data/data.hpp"

#include "app/config.hpp"

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>

namespace npps::data {

// ensure_no_conflict
template <typename T> static void ensure_no_conflict(const std::vector<T> &items) {
    std::unordered_map<int64_t, std::string> ids;
    for (const T &item : items) {
        int64_t item_id = schema::_internal_id(item);
        auto it = ids.find(item_id);
        if (it != ids.end())
            throw std::runtime_error("conflict item id string between '" + item.id_string + "' and '" + it->second +
                                     "' (id number is " + std::to_string(item_id) + ")");
        ids.emplace(item_id, item.id_string);
    }
}

static std::unique_ptr<ServerData> build(schema::SerializedServerData serialized) {
    auto d = std::make_unique<ServerData>();
    d->json_schema_link = serialized.json_schema_link;
    for (const std::string &b : serialized.badwords) d->badwords.push_back(util::base64_decode(b));
    for (auto &k : serialized.achievement_reward) d->achievement_reward.set(k.achievement_id, std::move(k.rewards));
    d->live_unit_drop_chance = serialized.live_unit_drop_chance;
    d->common_live_unit_drops = std::move(serialized.common_live_unit_drops);
    for (auto &k : serialized.live_specific_live_unit_drops)
        d->live_specific_live_unit_drops.set(k.live_setting_id, std::move(k.drops));
    for (auto &k : serialized.live_effort_drops) d->live_effort_drops.set(k.live_effort_point_box_spec_id, std::move(k.drops));
    ensure_no_conflict(serialized.secretbox_data);
    for (auto &sb : serialized.secretbox_data) {
        int64_t id = schema::secretbox_id(sb);
        d->secretbox_data.set(id, std::move(sb));
    }
    d->serial_codes = std::move(serialized.serial_codes);
    ensure_no_conflict(serialized.sticker_shop);
    d->sticker_shop = std::move(serialized.sticker_shop);
    return d;
}

// Old versions are never freed: callers hold plain references.
static std::vector<std::unique_ptr<ServerData>> versions;
static ServerData *server_data = nullptr;
static int64_t last_server_data_timestamp = 0;

static int64_t file_mtime_ns(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) throw std::runtime_error("cannot stat " + path);
#ifdef __vita__
    return (int64_t)st.st_mtime * 1000000000LL;
#else
    return (int64_t)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
#endif
}

const ServerData &get() {
    const std::string &path = config::get().server_data;
    int64_t mtime = file_mtime_ns(path);
    if (!server_data || mtime > last_server_data_timestamp) {
        try {
            std::ifstream f(path, std::ios::binary);
            if (!f) throw std::runtime_error("cannot open " + path);
            std::stringstream ss;
            ss << f.rdbuf();
            auto parsed = model::parse<schema::SerializedServerData>(Json::parse(ss.str()));
            versions.push_back(build(std::move(parsed)));
            server_data = versions.back().get();
        } catch (const std::exception &e) {
            if (!server_data) throw;
            util::log("Cannot load new data, using old data for now: %s", e.what());
        }
        last_server_data_timestamp = mtime;
    }
    return *server_data;
}

// json.dumps(obj, ensure_ascii=False, indent="\t")
static void dump_indent(const Json &j, std::string &out, int depth) {
    auto pad = [&](int d) { out.append((size_t)d, '\t'); };
    switch (j.type()) {
    case Json::Array:
        if (j.empty()) { out += "[]"; return; }
        out += "[\n";
        for (size_t i = 0; i < j.size(); i++) {
            pad(depth + 1);
            dump_indent(j[i], out, depth + 1);
            out += i + 1 < j.size() ? ",\n" : "\n";
        }
        pad(depth);
        out += ']';
        return;
    case Json::Object: {
        if (j.empty()) { out += "{}"; return; }
        out += "{\n";
        size_t i = 0;
        for (const auto &m : j.members()) {
            pad(depth + 1);
            // Keys are plain ASCII identifiers here; strings keep their UTF-8.
            dump_indent(Json(m.first), out, depth + 1);
            out += ": ";
            dump_indent(m.second, out, depth + 1);
            out += ++i < j.size() ? ",\n" : "\n";
        }
        pad(depth);
        out += '}';
        return;
    }
    case Json::String: {
        out += '"';
        for (char c : j.as_string()) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if ((unsigned char)c < 0x20) out += util::format("\\u%04x", (unsigned)c);
                else out += c;
            }
        }
        out += '"';
        return;
    }
    default: j.dump_to(out);
    }
}

void update() {
    const ServerData &sd = get();
    schema::SerializedServerData s;
    s.json_schema_link = sd.json_schema_link;
    for (const std::string &b : sd.badwords) s.badwords.push_back(util::base64_urlsafe_encode(b));
    for (const auto &kv : sd.achievement_reward) {
        schema::AchievementReward r;
        r.achievement_id = kv.first;
        r.rewards = kv.second;
        s.achievement_reward.push_back(std::move(r));
    }
    std::stable_sort(s.achievement_reward.begin(), s.achievement_reward.end(),
                     [](const auto &a, const auto &b) { return a.achievement_id < b.achievement_id; });
    s.live_unit_drop_chance = sd.live_unit_drop_chance;
    s.common_live_unit_drops = sd.common_live_unit_drops;
    for (const auto &kv : sd.live_specific_live_unit_drops) {
        schema::LiveSpecificLiveUnitDrop r;
        r.live_setting_id = kv.first;
        r.drops = kv.second;
        s.live_specific_live_unit_drops.push_back(std::move(r));
    }
    for (const auto &kv : sd.live_effort_drops) {
        schema::LiveEffortRewardDrops r;
        r.live_effort_point_box_spec_id = kv.first;
        r.drops = kv.second;
        s.live_effort_drops.push_back(std::move(r));
    }
    for (const auto &kv : sd.secretbox_data) s.secretbox_data.push_back(kv.second);
    ensure_no_conflict(s.secretbox_data);
    s.serial_codes = sd.serial_codes;
    s.sticker_shop = sd.sticker_shop;
    ensure_no_conflict(s.sticker_shop);

    // NOTE: unlike the Python version, defaults are not excluded from the output.
    std::string encoded;
    dump_indent(s.to_json(), encoded, 0);

    const std::string &path = config::get().server_data;
    std::string temp = path + "temp_" + std::to_string(util::monotonic_us()) + ".json";
    {
        std::ofstream f(temp, std::ios::binary);
        f << encoded;
    }
    if (std::rename(temp.c_str(), path.c_str()) != 0) throw std::runtime_error("cannot replace " + path);
    last_server_data_timestamp = file_mtime_ns(path);
}

}  // namespace npps::data
