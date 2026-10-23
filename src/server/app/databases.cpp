#include "databases.hpp"

#include "app/config.hpp"
#include "core/honky.hpp"

#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <memory>

namespace npps::app {

FILE *(*open_game_file)(const char *path, const char *mode) = fopen;

namespace {

const char *const MASTER_NAMES[] = {"game_mater", "item", "live", "unit", "achievement", "effort",
                                    "subscenario", "museum", "scenario", "exchange"};

struct Handles {
    std::unique_ptr<db::Database> main;
    std::unique_ptr<db::Session> main_session;
    std::unique_ptr<db::Database> master[10];
    std::unique_ptr<db::ReadSession> master_session[10];
    std::unique_ptr<Databases> view;
};
Handles handles;

bool file_size(const std::string &path, int64_t &size) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    size = st.st_size;
    return true;
}

void make_dirs(const std::string &path) {
    for (size_t i = 1; i <= path.size(); i++)
        if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0777);
}

// Returns a path SQLite can open for master database `name`.
std::string prepare_master(const char *name) {
    const config::Config &c = config::get();
    std::string file = std::string(name) + ".db_";
    std::string client_dir = strcmp(name, "game_mater") == 0 ? "common" : name;
    std::string source;
    FILE *in = nullptr;
    for (const std::string &dir : c.master_db_directories) {
        for (const std::string &candidate : {dir + "/" + file, dir + "/" + client_dir + "/" + file}) {
            in = open_game_file(candidate.c_str(), "rb");
            if (in) {
                source = candidate;
                break;
            }
        }
        if (in) break;
    }
    if (!in) throw db::DbError("cannot find master database " + file);
    unsigned char header[honky::HEADER_SIZE];
    size_t got = fread(header, 1, sizeof(header), in);
    if (honky::is_sqlite(header, got) && open_game_file == fopen) {
        fclose(in);
        return source;
    }
    fseek(in, 0, SEEK_END);
    int64_t source_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    std::string dir = c.data_directory + "/db-cache";
    make_dirs(dir);
    std::string target = dir + "/" + name + ".db";
    std::string stamp_path = target + ".source-size";
    int64_t existing;
    if (file_size(target, existing)) {
        if (FILE *s = fopen(stamp_path.c_str(), "rb")) {
            long long recorded = -1;
            if (fscanf(s, "%lld", &recorded) != 1) recorded = -1;
            fclose(s);
            if (recorded == source_size) {
                fclose(in);
                return target;
            }
        }
    }

    honky::Context ctx{};
    bool encrypted = false;
    if (!honky::is_sqlite(header, got)) {
        if (got != sizeof(header) || !honky::decrypt_setup(std::string(name) + ".db_", header, ctx)) {
            fclose(in);
            throw db::DbError(std::string("unrecognised master database ") + source);
        }
        encrypted = true;
        fseek(in, (long)honky::HEADER_SIZE, SEEK_SET);
    } else
        fseek(in, 0, SEEK_SET);

    std::string temp = target + ".tmp";
    FILE *out = fopen(temp.c_str(), "wb");
    if (!out) {
        fclose(in);
        throw db::DbError("cannot write " + temp + ": " + strerror(errno));
    }
    std::vector<unsigned char> buffer(256 * 1024);
    size_t n;
    bool ok = true;
    while ((n = fread(buffer.data(), 1, buffer.size(), in)) > 0) {
        if (encrypted) ctx.apply(buffer.data(), n);
        if (fwrite(buffer.data(), 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    fclose(in);
    if (fclose(out) != 0) ok = false;
    if (!ok) {
        remove(temp.c_str());
        throw db::DbError("cannot write " + temp);
    }
    remove(target.c_str());
    if (rename(temp.c_str(), target.c_str()) != 0) throw db::DbError("cannot rename " + temp);
    if (FILE *s = fopen(stamp_path.c_str(), "wb")) {
        fprintf(s, "%lld\n", (long long)source_size);
        fclose(s);
    }
    util::log("Decrypted master database %s", name);
    return target;
}

void create_schema(db::Database &main) {
    main.exec("BEGIN");
    for (const char *const *sql = MAIN_SCHEMA; *sql; sql++) main.exec(*sql);
    main.execute("INSERT INTO alembic_version (version_num) VALUES (?)", std::string(MAIN_SCHEMA_REVISION));
    main.exec("COMMIT");
}

}  // namespace

void open_databases() {
    db::initialize();
    const config::Config &c = config::get();
    make_dirs(c.data_directory);
    std::string main_path = c.data_directory + "/main.sqlite3";
    int64_t size;
    bool fresh = !file_size(main_path, size) || size == 0;
    handles.main = std::make_unique<db::Database>(main_path, false);
    db::Database &main = *handles.main;
#ifdef __vita__
    // No shared memory on the Vita VFS: WAL needs exclusive locking.
    main.exec("PRAGMA locking_mode=EXCLUSIVE");
#endif
    main.exec("PRAGMA journal_mode=WAL");
    main.exec("PRAGMA synchronous=NORMAL");
    main.exec("PRAGMA wal_autocheckpoint=1000");
    main.exec("PRAGMA cache_size=-4096");
    if (fresh) create_schema(main);
    else {
        db::Stmt &s = main.prepare("SELECT version_num FROM alembic_version");
        std::string revision = s.step() ? s.get_text(0) : "";
        s.reset();
        if (revision != MAIN_SCHEMA_REVISION)
            throw db::DbError("main database is at revision " + revision + ", expected " + MAIN_SCHEMA_REVISION +
                              "; migrate it with NPPS4 first");
    }
    handles.main_session = std::make_unique<db::Session>(main);

    for (int i = 0; i < 10; i++) {
        handles.master[i] = std::make_unique<db::Database>(prepare_master(MASTER_NAMES[i]), true);
#ifdef __vita__
        // Master rows are cached by ReadSession; keep SQLite's pages small next to the game.
        handles.master[i]->exec("PRAGMA cache_size=-256");
#else
        handles.master[i]->exec("PRAGMA cache_size=-1024");
#endif
        handles.master_session[i] = std::make_unique<db::ReadSession>(*handles.master[i]);
    }
    auto &m = handles.master_session;
    handles.view.reset(new Databases{*handles.main_session, *m[0], *m[1], *m[2], *m[3], *m[4], *m[5], *m[6], *m[7],
                                     *m[8], *m[9]});
}

void close_databases() {
    handles.view.reset();
    for (int i = 0; i < 10; i++) {
        handles.master_session[i].reset();
        handles.master[i].reset();
    }
    handles.main_session.reset();
    if (handles.main) {
        try {
            handles.main->exec("PRAGMA wal_checkpoint(TRUNCATE)");
        } catch (...) {
        }
        handles.main.reset();
    }
}

}  // namespace npps::app

namespace npps {
Databases &databases() {
    if (!app::handles.view) throw std::runtime_error("databases are not open");
    return *app::handles.view;
}
}  // namespace npps
