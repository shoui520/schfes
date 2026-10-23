// external/beatmap.py
#include "external/beatmap.hpp"

#include "app/config.hpp"
#include "core/json.hpp"
#include "core/util.hpp"

#include <zlib.h>

#include <cstring>
#include <fstream>
#include <unordered_map>
#include <sstream>

namespace npps::external::beatmap {

static std::optional<std::string> read_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    if (f.bad()) return std::nullopt;
    return ss.str();
}

static std::optional<std::string> gunzip(const std::string &data) {
    z_stream zs{};
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) return std::nullopt;
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = (uInt)data.size();
    std::string out;
    char buf[32768];
    int rc;
    do {
        zs.next_out = (Bytef *)buf;
        zs.avail_out = sizeof(buf);
        rc = inflate(&zs, Z_NO_FLUSH);
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (rc == Z_OK);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) return std::nullopt;
    return out;
}

// Index of a tools/npps_beatmaps.py archive: livejson -> (offset, size) of its gzip data.
using PackIndex = std::unordered_map<std::string, std::pair<uint32_t, uint32_t>>;

static uint32_t read_u32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static const PackIndex &pack_index(const std::string &path) {
    static PackIndex index;
    static bool loaded = false;
    if (loaded) return index;
    loaded = true;
    std::ifstream f(path, std::ios::binary);
    unsigned char head[12];
    if (!f.read((char *)head, sizeof(head)) || memcmp(head, "NPPSBMP1", 8) != 0) {
        util::log("beatmap pack %s is missing or invalid", path.c_str());
        return index;
    }
    uint32_t count = read_u32(head + 8);
    index.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        unsigned char len[2], entry[8];
        if (!f.read((char *)len, 2)) break;
        std::string name(len[0] | len[1] << 8, '\0');
        if (!f.read(&name[0], (std::streamsize)name.size()) || !f.read((char *)entry, 8)) break;
        index.emplace(std::move(name), std::make_pair(read_u32(entry), read_u32(entry + 4)));
    }
    return index;
}

static std::optional<std::string> read_packed(const std::string &path, const std::string &livejson) {
    const PackIndex &index = pack_index(path);
    auto it = index.find(livejson);
    if (it == index.end()) return std::nullopt;
    std::ifstream f(path, std::ios::binary);
    std::string data(it->second.second, '\0');
    if (!f.seekg(it->second.first) || !f.read(&data[0], (std::streamsize)data.size())) return std::nullopt;
    return gunzip(data);
}

// The beatmap pack when configured (the Vita build), otherwise beatmaps/<livejson>.
std::optional<std::string> load_beatmap_source(const std::string &livejson) {
    const config::Config &c = config::get();
    if (!c.beatmap_pack.empty()) return read_packed(c.beatmap_pack, livejson);
    return read_file(c.beatmap_directory + "/" + livejson);
}

// BeatmapData.model_validate; nullopt on a validation error.
static std::optional<BeatmapData> validate(const Json &j) {
    if (!j.is_object()) return std::nullopt;
    try {
        BeatmapData b;
        const char *required[] = {"timing_sec", "notes_attribute", "notes_level", "effect", "effect_value", "position"};
        for (const char *key : required)
            if (!j.contains(key)) return std::nullopt;
        b.timing_sec = j.at("timing_sec").to_double();
        b.notes_attribute = j.at("notes_attribute").to_int();
        b.notes_level = j.at("notes_level").to_int();
        b.effect = j.at("effect").to_int();
        b.effect_value = j.at("effect_value").to_double();
        b.position = j.at("position").to_int();
        if (const Json *v = j.find("speed")) b.speed = v->to_double();
        if (const Json *v = j.find("vanish")) {
            if (!v->is_int() || v->as_int() < 0 || v->as_int() > 2) return std::nullopt;
            b.vanish = v->as_int();
        }
        return b;
    } catch (const JsonError &) {
        return std::nullopt;
    }
}

std::optional<std::vector<BeatmapData>> get_beatmap_data(const std::string &livejson, Context &) {
    std::optional<std::string> source = load_beatmap_source(livejson);
    if (!source) return std::nullopt;

    Json jsondata;
    try {
        jsondata = Json::parse(*source);
    } catch (const JsonError &) {
        return std::nullopt;
    }
    if (!jsondata.is_array()) return std::nullopt;

    std::vector<BeatmapData> result;
    result.reserve(jsondata.size());
    for (const Json &jdata : jsondata.elements()) {
        std::optional<BeatmapData> beatmap = validate(jdata);
        if (!beatmap) return std::nullopt;
        result.push_back(*beatmap);
    }
    return result;
}

std::vector<BeatmapData> randomize_beatmaps(const std::vector<BeatmapData> &beatmap, const Bytes &, Context &) {
    // TODO: https://github.com/DarkEnergyProcessor/livesim2_async/blob/over_the_rainbow/game/live/randomizer3.lua
    return beatmap;
}

}  // namespace npps::external::beatmap
