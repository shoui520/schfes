// Vita entry points for the in-process server (src/npps_server.h).
#include "npps_server.h"

#include "app/config.hpp"
#include "app/databases.hpp"
#include "app/server.hpp"
#include "core/crypto.hpp"
#include "core/honky.hpp"
#include "core/http.hpp"
#include "core/json.hpp"

#include <psp2/kernel/threadmgr.h>

#include <cstdlib>
#include <cstring>
#include <memory>

extern "C" {
void sif_log(const char *fmt, ...);
FILE *fopen_soloader(const char *filename, const char *mode);
}

// Generated at build time from NPPS4's default_server_key.pem.
extern const char npps_embedded_server_key[];

namespace {

std::unique_ptr<npps::http::Server> server;

void log_line(const char *line) { sif_log("[NPPS] %s", line); }

int server_thread(SceSize, void *) {
    try {
        npps::app::initialize();
        npps::util::log("server ready on 127.0.0.1:%d", server->port());
        server->run(npps::app::handle_request);
    } catch (const std::exception &e) {
        npps::util::log("server stopped: %s", e.what());
    }
    return 0;
}

}  // namespace

extern "C" int sif_server_start(void) {
    try {
        npps::util::set_log_sink(log_line);
        npps::config::Config &c = npps::config::get();
        c.data_directory = "ux0:/data/sif/server";
        // The game's own copies: downloaded updates in sif-external, the base install in sif-install.
        c.master_db_directories = {"ux0:/data/sif/sif-external/db", "ux0:/data/sif/sif-install/db"};
        c.server_data = "app0:npps/server_data.json";
        c.beatmap_pack = "app0:npps/beatmaps.bin";
        npps::app::open_game_file = fopen_soloader;
        npps::crypto::load_server_key(npps_embedded_server_key);
        server = std::make_unique<npps::http::Server>("127.0.0.1", SIF_SERVER_PORT);
    } catch (const std::exception &e) {
        sif_log("[NPPS] cannot start: %s", e.what());
        return -1;
    }
    SceUID thread = sceKernelCreateThread("npps_server", server_thread, 0x10000100 /* default user priority */, 1024 * 1024,
                                          0, SIF_SERVER_CPU_MASK, nullptr);
    if (thread < 0 || sceKernelStartThread(thread, 0, nullptr) < 0) {
        sif_log("[NPPS] cannot create the server thread: 0x%08x", (unsigned)thread);
        return -1;
    }
    return 0;
}

extern "C" int sif_server_info_file(unsigned char **out, size_t *size) {
    std::string base = "http://127.0.0.1:" + std::to_string(SIF_SERVER_PORT);
    npps::Json api = npps::Json::object();
    for (const char *path : {"/battle/startWait", "/battle/endWait", "/duty/startWait", "/duty/endWait",
                             "/duty/privateStartWait"})
        api[path] = base + "/main.php" + path;
    npps::Json info = npps::Json::obj({
        {"name", "server_information"},
        {"domain", base},
        {"maintenance_uri", base + "/resources/maintenance/maintenance.php"},
        {"update_uri", base + "/resources/maintenance/update.php"},
        {"login_news_uri", base + "/webview.php/announce/index"},
        {"locked_user_uri", base + "/webview.php/static/index?id=13"},
        {"server_version", npps::util::sif_version_string(npps::config::get_latest_version())},
        {"end_point", "/main.php"},
        {"consumer_key", npps::config::get_consumer_key()},
        {"application_id", "626776655"},
        {"max_connection", 10},
        {"region", "392"},
        {"date", "1676991600"},
        {"application_key", npps::config::get_application_key()},
        {"api_uri", std::move(api)},
    });
    std::string json = info.dump();
    unsigned char *buffer = (unsigned char *)malloc(npps::honky::HEADER_SIZE + json.size());
    if (!buffer) return -1;
    npps::honky::Context ctx{};
    npps::honky::encrypt_setup_v3("server_info.json", buffer, ctx);
    memcpy(buffer + npps::honky::HEADER_SIZE, json.data(), json.size());
    ctx.apply(buffer + npps::honky::HEADER_SIZE, json.size());
    *out = buffer;
    *size = npps::honky::HEADER_SIZE + json.size();
    return 0;
}
