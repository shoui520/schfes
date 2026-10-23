#pragma once
// external/beatmap.py: the default beatmap provider. Beatmaps are <config beatmap_directory>/<livejson> JSON files,
// or entries of the config beatmap_pack archive.
#include "app/context.hpp"

#include <optional>
#include <string>
#include <vector>

namespace npps::external::beatmap {

struct BeatmapData {
    double timing_sec = 0;
    int64_t notes_attribute = 0;
    int64_t notes_level = 0;
    int64_t effect = 0;
    double effect_value = 0;
    int64_t position = 0;
    double speed = 1.0;  // Beatmap speed multipler
    int64_t vanish = 0;  // 0 = Normal; 1 = Note hidden as it approaches; 2 = Note shows just before its timing.
};

// Raw beatmap text for `livejson`, or nullopt when it cannot be read. The single place that touches storage.
std::optional<std::string> load_beatmap_source(const std::string &livejson);

// nullopt if the beatmap is not found or invalid.
std::optional<std::vector<BeatmapData>> get_beatmap_data(const std::string &livejson, Context &context);
// seed has length 16.
std::vector<BeatmapData> randomize_beatmaps(const std::vector<BeatmapData> &beatmap, const Bytes &seed,
                                            Context &context);

}  // namespace npps::external::beatmap
