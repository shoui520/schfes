#pragma once
#include "util.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

struct sqlite3;
struct sqlite3_stmt;

namespace npps::db {

class DbError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Stmt {
public:
    Stmt() = default;
    explicit Stmt(sqlite3_stmt *s) : s_(s) {}
    Stmt(Stmt &&o) noexcept : s_(o.s_) { o.s_ = nullptr; }
    Stmt &operator=(Stmt &&o) noexcept;
    Stmt(const Stmt &) = delete;
    ~Stmt();

    void reset();
    // Returns true while a row is available.
    bool step();

    void bind(int i, std::nullptr_t);
    void bind(int i, int v) { bind(i, (long long)v); }
    void bind(int i, unsigned v) { bind(i, (long long)v); }
    void bind(int i, long v) { bind(i, (long long)v); }
    void bind(int i, long long v);
    void bind(int i, unsigned long v) { bind(i, (long long)v); }
    void bind(int i, unsigned long long v) { bind(i, (long long)v); }
    void bind(int i, bool v) { bind(i, (long long)v); }
    void bind(int i, double v);
    void bind(int i, const char *v) { bind(i, std::string_view(v)); }
    void bind(int i, const std::string &v) { bind(i, std::string_view(v)); }
    void bind(int i, std::string_view v);
    void bind_blob(int i, std::string_view v);
    template <typename T> void bind(int i, const std::optional<T> &v) {
        if (v) bind(i, *v);
        else bind(i, nullptr);
    }
    template <typename... A> void bind_all(const A &...args) {
        int i = 1;
        (bind(i++, args), ...);
    }

    int columns() const;
    bool is_null(int i) const;
    int64_t get_int(int i) const;
    double get_double(int i) const;
    std::string get_text(int i) const;
    std::string get_blob(int i) const;
    std::optional<int64_t> get_opt_int(int i) const;
    std::optional<double> get_opt_double(int i) const;
    std::optional<std::string> get_opt_text(int i) const;
    std::optional<std::string> get_opt_blob(int i) const;
    void read(int i, int64_t &v) const { v = get_int(i); }
    void read(int i, int &v) const { v = (int)get_int(i); }
    void read(int i, bool &v) const { v = get_int(i) != 0; }
    void read(int i, double &v) const { v = get_double(i); }
    void read(int i, std::string &v) const { v = get_text(i); }
    void read(int i, std::optional<int64_t> &v) const { v = get_opt_int(i); }
    void read(int i, std::optional<bool> &v) const {
        if (is_null(i)) v.reset();
        else v = get_int(i) != 0;
    }
    void read(int i, std::optional<double> &v) const { v = get_opt_double(i); }
    void read(int i, std::optional<std::string> &v) const { v = get_opt_text(i); }
    sqlite3_stmt *raw() const { return s_; }

private:
    sqlite3_stmt *s_ = nullptr;
};

class Database {
public:
    // Opens a database file; read_only databases are never written.
    Database(const std::string &path, bool read_only);
    ~Database();
    Database(const Database &) = delete;

    void exec(const char *sql);
    // Statements are cached per SQL text and reset before reuse.
    Stmt &prepare(const std::string &sql);
    int64_t last_insert_rowid() const;
    int changes() const;
    bool in_transaction() const;
    void begin();
    void commit();
    void rollback();
    const std::string &path() const { return path_; }
    sqlite3 *raw() const { return db_; }

    template <typename... A> void execute(const std::string &sql, const A &...args) {
        Stmt &s = prepare(sql);
        s.bind_all(args...);
        while (s.step()) {
        }
        s.reset();
    }
    template <typename... A> std::optional<int64_t> scalar_int(const std::string &sql, const A &...args) {
        Stmt &s = prepare(sql);
        s.bind_all(args...);
        std::optional<int64_t> v;
        if (s.step()) v = s.get_opt_int(0);
        s.reset();
        return v;
    }

private:
    sqlite3 *db_ = nullptr;
    std::string path_;
    std::unordered_map<std::string, std::unique_ptr<Stmt>> cache_;
};

// Registers the Vita I/O VFS (no-op on other platforms).
void initialize();

}  // namespace npps::db
