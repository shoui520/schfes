#include "sqlite.hpp"

#include <sqlite3.h>

#include <cstring>

#ifdef __vita__
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#endif

namespace npps::db {

static void check(int rc, sqlite3 *db, const char *what) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
        throw DbError(std::string(what) + ": " + (db ? sqlite3_errmsg(db) : sqlite3_errstr(rc)));
}

Stmt &Stmt::operator=(Stmt &&o) noexcept {
    if (this != &o) {
        if (s_) sqlite3_finalize(s_);
        s_ = o.s_;
        o.s_ = nullptr;
    }
    return *this;
}
Stmt::~Stmt() {
    if (s_) sqlite3_finalize(s_);
}
void Stmt::reset() {
    sqlite3_reset(s_);
    sqlite3_clear_bindings(s_);
}
bool Stmt::step() {
    int rc = sqlite3_step(s_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    sqlite3 *db = sqlite3_db_handle(s_);
    std::string message = std::string(sqlite3_errmsg(db)) + " in: " + sqlite3_sql(s_);
    sqlite3_reset(s_);
    throw DbError(message);
}
void Stmt::bind(int i, std::nullptr_t) { check(sqlite3_bind_null(s_, i), sqlite3_db_handle(s_), "bind"); }
void Stmt::bind(int i, long long v) { check(sqlite3_bind_int64(s_, i, v), sqlite3_db_handle(s_), "bind"); }
void Stmt::bind(int i, double v) { check(sqlite3_bind_double(s_, i, v), sqlite3_db_handle(s_), "bind"); }
void Stmt::bind(int i, std::string_view v) {
    check(sqlite3_bind_text(s_, i, v.data(), (int)v.size(), SQLITE_TRANSIENT), sqlite3_db_handle(s_), "bind");
}
void Stmt::bind_blob(int i, std::string_view v) {
    check(sqlite3_bind_blob(s_, i, v.data(), (int)v.size(), SQLITE_TRANSIENT), sqlite3_db_handle(s_), "bind");
}
int Stmt::columns() const { return sqlite3_column_count(s_); }
bool Stmt::is_null(int i) const { return sqlite3_column_type(s_, i) == SQLITE_NULL; }
int64_t Stmt::get_int(int i) const { return sqlite3_column_int64(s_, i); }
double Stmt::get_double(int i) const { return sqlite3_column_double(s_, i); }
std::string Stmt::get_text(int i) const {
    const unsigned char *t = sqlite3_column_text(s_, i);
    return t ? std::string((const char *)t, sqlite3_column_bytes(s_, i)) : std::string();
}
std::string Stmt::get_blob(int i) const {
    const void *b = sqlite3_column_blob(s_, i);
    return b ? std::string((const char *)b, sqlite3_column_bytes(s_, i)) : std::string();
}
std::optional<int64_t> Stmt::get_opt_int(int i) const {
    if (is_null(i)) return std::nullopt;
    return get_int(i);
}
std::optional<double> Stmt::get_opt_double(int i) const {
    if (is_null(i)) return std::nullopt;
    return get_double(i);
}
std::optional<std::string> Stmt::get_opt_text(int i) const {
    if (is_null(i)) return std::nullopt;
    return get_text(i);
}
std::optional<std::string> Stmt::get_opt_blob(int i) const {
    if (is_null(i)) return std::nullopt;
    return get_blob(i);
}

Database::Database(const std::string &path, bool read_only) : path_(path) {
    int flags = read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    int rc = sqlite3_open_v2(path.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::string message = "cannot open " + path + ": " + (db_ ? sqlite3_errmsg(db_) : sqlite3_errstr(rc));
        sqlite3_close(db_);
        db_ = nullptr;
        throw DbError(message);
    }
}

Database::~Database() {
    cache_.clear();
    sqlite3_close(db_);
}

void Database::exec(const char *sql) {
    char *error = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (rc != SQLITE_OK) {
        std::string message = std::string("exec failed: ") + (error ? error : sqlite3_errstr(rc)) + " in: " + sql;
        sqlite3_free(error);
        throw DbError(message);
    }
}

Stmt &Database::prepare(const std::string &sql) {
    auto it = cache_.find(sql);
    if (it != cache_.end()) {
        it->second->reset();
        return *it->second;
    }
    sqlite3_stmt *s = nullptr;
    int rc = sqlite3_prepare_v3(db_, sql.c_str(), (int)sql.size(), SQLITE_PREPARE_PERSISTENT, &s, nullptr);
    if (rc != SQLITE_OK) throw DbError(std::string("prepare failed: ") + sqlite3_errmsg(db_) + " in: " + sql);
    auto &slot = cache_[sql];
    slot = std::make_unique<Stmt>(s);
    return *slot;
}

int64_t Database::last_insert_rowid() const { return sqlite3_last_insert_rowid(db_); }
int Database::changes() const { return sqlite3_changes(db_); }
bool Database::in_transaction() const { return !sqlite3_get_autocommit(db_); }
void Database::begin() { prepare("BEGIN").step(); }
void Database::commit() { prepare("COMMIT").step(); }
void Database::rollback() {
    if (in_transaction()) prepare("ROLLBACK").step();
}

#ifdef __vita__
// Minimal VFS on SceIo. The server is the only user of its files, so locks
// are no-ops and WAL runs in exclusive locking mode (no shared memory).
namespace {
struct VitaFile {
    sqlite3_file base;
    SceUID fd;
};

int x_close(sqlite3_file *f) {
    sceIoClose(((VitaFile *)f)->fd);
    return SQLITE_OK;
}
int x_read(sqlite3_file *f, void *buf, int n, sqlite3_int64 off) {
    int r = sceIoPread(((VitaFile *)f)->fd, buf, n, off);
    if (r < 0) return SQLITE_IOERR_READ;
    if (r < n) {
        memset((char *)buf + r, 0, n - r);
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}
int x_write(sqlite3_file *f, const void *buf, int n, sqlite3_int64 off) {
    return sceIoPwrite(((VitaFile *)f)->fd, buf, n, off) == n ? SQLITE_OK : SQLITE_IOERR_WRITE;
}
int x_truncate(sqlite3_file *f, sqlite3_int64 size) {
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    st.st_size = size;
    return sceIoChstatByFd(((VitaFile *)f)->fd, &st, SCE_CST_SIZE) < 0 ? SQLITE_IOERR_TRUNCATE : SQLITE_OK;
}
int x_sync(sqlite3_file *f, int) { return sceIoSyncByFd(((VitaFile *)f)->fd, 0) < 0 ? SQLITE_IOERR_FSYNC : SQLITE_OK; }
int x_size(sqlite3_file *f, sqlite3_int64 *size) {
    SceIoStat st;
    if (sceIoGetstatByFd(((VitaFile *)f)->fd, &st) < 0) return SQLITE_IOERR_FSTAT;
    *size = st.st_size;
    return SQLITE_OK;
}
int x_lock(sqlite3_file *, int) { return SQLITE_OK; }
int x_reserved(sqlite3_file *, int *out) {
    *out = 0;
    return SQLITE_OK;
}
int x_control(sqlite3_file *, int, void *) { return SQLITE_NOTFOUND; }
int x_sector(sqlite3_file *) { return 512; }
int x_device(sqlite3_file *) { return 0; }

const sqlite3_io_methods METHODS = {1, x_close, x_read, x_write, x_truncate, x_sync, x_size,
                                    x_lock, x_lock, x_reserved, x_control, x_sector, x_device};

int x_open(sqlite3_vfs *, const char *name, sqlite3_file *f, int flags, int *out_flags) {
    VitaFile *file = (VitaFile *)f;
    file->base.pMethods = nullptr;
    if (!name) return SQLITE_IOERR;  // temp files are kept in memory (SQLITE_TEMP_STORE=3)
    int mode = flags & SQLITE_OPEN_READWRITE ? SCE_O_RDWR : SCE_O_RDONLY;
    if (flags & SQLITE_OPEN_CREATE) mode |= SCE_O_CREAT;
    SceUID fd = sceIoOpen(name, mode, 0666);
    if (fd < 0) return SQLITE_CANTOPEN;
    file->fd = fd;
    file->base.pMethods = &METHODS;
    if (out_flags) *out_flags = flags;
    return SQLITE_OK;
}
int x_delete(sqlite3_vfs *, const char *name, int) {
    int r = sceIoRemove(name);
    return r < 0 && (unsigned)r != 0x80010002u ? SQLITE_IOERR_DELETE : SQLITE_OK;
}
int x_access(sqlite3_vfs *, const char *name, int, int *out) {
    SceIoStat st;
    *out = sceIoGetstat(name, &st) >= 0;
    return SQLITE_OK;
}
int x_fullpath(sqlite3_vfs *, const char *name, int n, char *out) {
    snprintf(out, n, "%s", name);
    return SQLITE_OK;
}
int x_random(sqlite3_vfs *, int n, char *out) {
    Bytes b = util::randbytes(n);
    memcpy(out, b.data(), n);
    return n;
}
int x_sleep(sqlite3_vfs *, int us) {
    sceKernelDelayThread(us);
    return us;
}
int x_time64(sqlite3_vfs *, sqlite3_int64 *out) {
    // Julian day in milliseconds.
    *out = (sqlite3_int64)util::time() * 1000 + 210866760000000LL;
    return SQLITE_OK;
}
int x_time(sqlite3_vfs *v, double *out) {
    sqlite3_int64 ms;
    x_time64(v, &ms);
    *out = ms / 86400000.0;
    return SQLITE_OK;
}
int x_error(sqlite3_vfs *, int, char *) { return 0; }

sqlite3_vfs VFS = {2, sizeof(VitaFile), 1024, nullptr, "psp2", nullptr, x_open, x_delete, x_access, x_fullpath,
                   nullptr, nullptr, nullptr, nullptr, x_random, x_sleep, x_time, x_error, x_time64};
}  // namespace

extern "C" int sqlite3_os_init(void) { return sqlite3_vfs_register(&VFS, 1); }
extern "C" int sqlite3_os_end(void) { return SQLITE_OK; }
#endif

void initialize() { sqlite3_initialize(); }

}  // namespace npps::db
