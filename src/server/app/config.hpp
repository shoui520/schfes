#pragma once
// npps4/config: the server's settings. Values default to NPPS4's
// config.sample.toml; the host build can override them from NPPS4_CONFIG_*
// environment variables, the Vita build sets them in code.
#include "core/util.hpp"

#include <string>
#include <vector>
#include <utility>

namespace npps::config {

struct Config {
    std::string data_directory = "data";  // main.sqlite3, decrypted master DB cache
    // Directories searched in order for honky-encrypted or plain master databases, either flat
    // (<name>.db_, NPPS4) or in the client layout (<name>/<name>.db_, common/game_mater.db_).
    // NPPS4_PORT_MASTER_DB_DIR separates several with ';'.
    std::vector<std::string> master_db_directories;
    std::string server_data = "server_data.json";
    std::string beatmap_directory = "beatmaps";
    std::string beatmap_pack;  // tools/npps_beatmaps.py archive; used instead of beatmap_directory when set
    std::string secret_key = "Hello World";
    int64_t session_expiry = 259200;
    bool save_notes_list = false;
    std::pair<int, int> client_version = {59, 4};
    std::string base_xorpad = "eit4Ahph4aiX4ohmephuobei6SooX9xo";
    std::string application_key = "b6e6c940a93af2357ea3e0ace0b98afc";
    std::string consumer_key = "lovelive_test";
    bool verify_xmc = true;
    bool enable_export = false;
    bool enable_import = false;
    bool bypass_signature = false;
    double energy_multiplier = 1;
    double love_multiplier = 1;
    double secretbox_cost_multiplier = 1;
    bool maintenance = false;
};

Config &get();

inline std::pair<int, int> get_latest_version() { return get().client_version; }
inline bool is_maintenance() { return get().maintenance; }
inline bool need_xmc_verify() { return get().verify_xmc; }
inline const std::string &get_base_xorpad() { return get().base_xorpad; }
inline const std::string &get_application_key() { return get().application_key; }
inline const std::string &get_secret_key() { return get().secret_key; }
inline const std::string &get_consumer_key() { return get().consumer_key; }
inline int64_t get_session_expiry_time() { return get().session_expiry; }
inline bool is_account_export_enabled() { return get().enable_export; }
inline bool store_backup_of_notes_list() { return get().save_notes_list; }

// Applies NPPS4_CONFIG_* variables that this port understands.
void load_environment();

}  // namespace npps::config
