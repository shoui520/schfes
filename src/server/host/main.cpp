// Host build of the server for testing against Python NPPS4:
//   npps-host --port 51377 --key default_server_key.pem
// Settings come from NPPS4_CONFIG_* variables (see app/config.cpp).
#include "app/config.hpp"
#include "app/databases.hpp"
#include "app/idol.hpp"
#include "app/server.hpp"
#include "core/crypto.hpp"
#include "core/http.hpp"

#include <csignal>
#include <cstring>
#include <fstream>
#include <sstream>

static npps::http::Server *running;

int main(int argc, char **argv) {
    int port = 51377;
    const char *address = "127.0.0.1";
    std::string key_path = "default_server_key.pem";
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--port")) port = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "--address")) address = argv[i + 1];
        else if (!strcmp(argv[i], "--key")) key_path = argv[i + 1];
    }
    npps::config::load_environment();
    // Deterministic runs for differential testing against a patched NPPS4.
    if (const char *seed = getenv("NPPS4_TEST_SEED")) npps::util::seed_for_testing(strtoull(seed, nullptr, 10));
    if (const char *t = getenv("NPPS4_TEST_TIME")) npps::util::set_time_for_testing(strtoll(t, nullptr, 10));
    std::ifstream key_file(key_path);
    if (!key_file) {
        fprintf(stderr, "cannot read %s\n", key_path.c_str());
        return 1;
    }
    std::stringstream pem;
    pem << key_file.rdbuf();
    npps::crypto::load_server_key(pem.str());
    npps::app::initialize();
    npps::http::Server server(address, port);
    running = &server;
    signal(SIGINT, [](int) { running->stop(); });
    signal(SIGTERM, [](int) { running->stop(); });
    signal(SIGPIPE, SIG_IGN);
    npps::util::log("NPPS4 port listening on %s:%d", address, server.port());
    server.run(npps::app::handle_request);
    npps::app::shutdown();
    return 0;
}
