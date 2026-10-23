#include "config.hpp"

#include <cstdlib>
#include <cstring>

namespace npps::config {

Config &get() {
    static Config config;
    return config;
}

void load_environment() {
    Config &c = get();
    auto str = [](const char *name, std::string &out) {
        if (const char *v = getenv(name)) out = v;
    };
    auto boolean = [](const char *name, bool &out) {
        if (const char *v = getenv(name)) out = !strcmp(v, "1") || !strcasecmp(v, "true");
    };
    auto number = [](const char *name, double &out) {
        if (const char *v = getenv(name)) out = strtod(v, nullptr);
    };
    str("NPPS4_CONFIG_MAIN_DATADIR", c.data_directory);
    str("NPPS4_CONFIG_MAIN_SECRETKEY", c.secret_key);
    str("NPPS4_CONFIG_MAIN_SERVERDATA", c.server_data);
    if (const char *v = getenv("NPPS4_CONFIG_MAIN_TOKENEXPIRY")) c.session_expiry = strtoll(v, nullptr, 10);
    if (const char *v = getenv("NPPS4_CONFIG_DOWNLOAD_NONE_VERSION")) c.client_version = util::parse_sif_version(v);
    boolean("NPPS4_CONFIG_ADVANCED_XMC", c.verify_xmc);
    boolean("NPPS4_CONFIG_IEX_EXPORT", c.enable_export);
    boolean("NPPS4_CONFIG_IEX_IMPORT", c.enable_import);
    boolean("NPPS4_CONFIG_IEX_BYPASS", c.bypass_signature);
    number("NPPS4_CONFIG_GAMEPLAY_LPMUL", c.energy_multiplier);
    number("NPPS4_CONFIG_GAMEPLAY_LOVEMUL", c.love_multiplier);
    number("NPPS4_CONFIG_GAMEPLAY_GACHACOSTMUL", c.secretbox_cost_multiplier);
    if (const char *v = getenv("NPPS4_PORT_MASTER_DB_DIR")) {
        c.master_db_directories.clear();
        for (std::string_view rest = v; !rest.empty();) {
            size_t end = rest.find(';');
            if (end) c.master_db_directories.emplace_back(rest.substr(0, end));
            rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
        }
    }
    str("NPPS4_PORT_BEATMAP_DIR", c.beatmap_directory);
    str("NPPS4_PORT_BEATMAP_PACK", c.beatmap_pack);
}

}  // namespace npps::config
