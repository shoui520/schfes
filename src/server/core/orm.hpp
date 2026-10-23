#pragma once
// Small unit-of-work ORM over generated table structs, mirroring how NPPS4
// uses SQLAlchemy: rows loaded in a session are identity-mapped (one object
// per primary key), mutations are found by comparing against a snapshot,
// pending changes are flushed before every query (autoflush) and at commit.
//
// A table struct T provides (see db/main_tables.hpp, generated):
//   static constexpr const char *TABLE, *COLUMNS, *INSERT_SQL, *UPDATE_SQL, *PK;
//   static constexpr bool AUTO_PK;  using Key = int64_t or std::string;
//   Key pk() const; void set_pk(int64_t);
//   void read(const Stmt &, int first_column);
//   void bind_insert(Stmt &) const;  void bind_update(Stmt &) const;
//   void on_update();  bool operator==(const T &) const;
#include "sqlite.hpp"

#include <map>
#include <typeindex>
#include <vector>

namespace npps::db {

class Session;

struct TrackedBase {
    virtual ~TrackedBase() = default;
    enum State : uint8_t { Persistent, Pending, Deleted, Detached } state = Persistent;
    virtual bool dirty() const = 0;
    virtual const void *object() const = 0;
    virtual void insert(Database &db, Session &session) = 0;
    virtual void update(Database &db) = 0;
    virtual void remove(Database &db) = 0;
};

template <typename T> struct Tracked final : TrackedBase {
    T obj, snapshot;
    bool dirty() const override { return state == Persistent && !(obj == snapshot); }
    const void *object() const override { return &obj; }
    void insert(Database &db, Session &session) override;
    void update(Database &db) override {
        obj.on_update();
        Stmt &s = db.prepare(T::UPDATE_SQL);
        obj.bind_update(s);
        s.step();
        s.reset();
        snapshot = obj;
    }
    void remove(Database &db) override {
        Stmt &s = db.prepare(std::string("DELETE FROM \"") + T::TABLE + "\" WHERE \"" + T::PK + "\" = ?");
        s.bind(1, snapshot.pk());
        s.step();
        s.reset();
        state = Detached;
    }
};

class Session {
public:
    explicit Session(Database &db) : db_(db) {}
    Session(const Session &) = delete;
    ~Session() { rollback(); }

    Database &database() { return db_; }

    // session.get(T, pk)
    template <typename T> T *get(const typename T::Key &pk) {
        flush();
        auto &map = identity<T>();
        auto it = map.find(pk);
        if (it != map.end()) return it->second->state == TrackedBase::Persistent ? &it->second->obj : nullptr;
        std::vector<T *> rows = select<T>(std::string("WHERE \"") + T::PK + "\" = ?", pk);
        return rows.empty() ? nullptr : rows[0];
    }

    // SELECT <columns> FROM <table> <tail>; tail holds WHERE/ORDER BY/LIMIT.
    template <typename T, typename... A> std::vector<T *> select(const std::string &tail, const A &...args) {
        flush();
        begin_if_needed();
        Stmt &s = db_.prepare(std::string("SELECT ") + T::COLUMNS + " FROM \"" + T::TABLE + "\" " + tail);
        s.bind_all(args...);
        std::vector<T *> out;
        auto &map = identity<T>();
        T row;
        while (s.step()) {
            row.read(s, 0);
            auto it = map.find(row.pk());
            if (it != map.end()) {
                // Like SQLAlchemy, an already loaded object wins over the row.
                if (it->second->state == TrackedBase::Persistent) out.push_back(&it->second->obj);
                continue;
            }
            auto t = std::make_unique<Tracked<T>>();
            t->obj = row;
            t->snapshot = row;
            out.push_back(&t->obj);
            map.emplace(row.pk(), t.get());
            loaded_.push_back(t.get());
            owned_.push_back(std::move(t));
        }
        s.reset();
        return out;
    }

    template <typename T, typename... A> T *first(const std::string &tail, const A &...args) {
        std::vector<T *> rows = select<T>(tail + " LIMIT 1", args...);
        return rows.empty() ? nullptr : rows[0];
    }

    template <typename T, typename... A> int64_t count(const std::string &tail, const A &...args) {
        return scalar_int(std::string("SELECT COUNT(*) FROM \"") + T::TABLE + "\" " + tail, args...).value_or(0);
    }

    // Raw statement in the session's transaction, after flushing. Loaded
    // objects are not refreshed (SQLAlchemy core statement semantics).
    template <typename... A> int execute(const std::string &sql, const A &...args) {
        flush();
        begin_if_needed();
        db_.execute(sql, args...);
        return db_.changes();
    }

    template <typename... A> std::optional<int64_t> scalar_int(const std::string &sql, const A &...args) {
        flush();
        begin_if_needed();
        return db_.scalar_int(sql, args...);
    }

    // Prepared statement for custom queries (bind, step, read, reset).
    Stmt &query(const std::string &sql) {
        flush();
        begin_if_needed();
        return db_.prepare(sql);
    }

    // session.add(obj): inserted at the next flush; the returned pointer
    // stays valid for the session's lifetime.
    template <typename T> T *add(T obj) {
        auto t = std::make_unique<Tracked<T>>();
        t->obj = std::move(obj);
        t->state = TrackedBase::Pending;
        T *p = &t->obj;
        pending_.push_back(t.get());
        owned_.push_back(std::move(t));
        return p;
    }

    // session.delete(obj)
    template <typename T> void remove(T *obj) {
        for (TrackedBase *t : pending_)
            if (t->object() == obj && t->state == TrackedBase::Pending) {
                t->state = TrackedBase::Detached;
                return;
            }
        auto &map = identity<T>();
        auto it = map.find(obj->pk());
        if (it != map.end() && &it->second->obj == obj && it->second->state == TrackedBase::Persistent) {
            it->second->state = TrackedBase::Deleted;
            deleted_.push_back(it->second);
        }
    }

    void flush();
    void commit();
    void rollback();
    // Forgets every loaded object (the end of an SQLAlchemy session).
    void clear();

    template <typename T> std::map<typename T::Key, Tracked<T> *> &identity() {
        auto &slot = maps_[std::type_index(typeid(T))];
        if (!slot) slot = std::make_unique<Map<T>>();
        return static_cast<Map<T> *>(slot.get())->rows;
    }
    void track_loaded(TrackedBase *t) { loaded_.push_back(t); }

private:
    void begin_if_needed();

    struct MapBase {
        virtual ~MapBase() = default;
    };
    template <typename T> struct Map final : MapBase {
        std::map<typename T::Key, Tracked<T> *> rows;
    };

    Database &db_;
    std::map<std::type_index, std::unique_ptr<MapBase>> maps_;
    std::vector<TrackedBase *> loaded_, pending_, deleted_;
    std::vector<std::unique_ptr<TrackedBase>> owned_;
    bool flushing_ = false;
};

template <typename T> void Tracked<T>::insert(Database &db, Session &session) {
    Stmt &s = db.prepare(T::INSERT_SQL);
    obj.bind_insert(s);
    s.step();
    s.reset();
    if constexpr (T::AUTO_PK) obj.set_pk(db.last_insert_rowid());
    state = Persistent;
    snapshot = obj;
    session.identity<T>()[obj.pk()] = this;
    session.track_loaded(this);
}

// Read-only session over a master database. Rows live until the server
// stops, so repeated lookups across requests do not touch SQLite; tables
// without a single-column key are kept only until end_request().
class ReadSession {
public:
    explicit ReadSession(Database &db) : db_(db) {}
    ReadSession(const ReadSession &) = delete;

    Database &database() { return db_; }

    template <typename T> const T *get(const typename T::Key &pk) {
        auto &map = rows<T>();
        auto it = map.find(pk);
        if (it != map.end()) return it->second.get();
        std::vector<const T *> found = select<T>(std::string("WHERE \"") + T::PK + "\" = ?", pk);
        if (found.empty()) {
            map.emplace(pk, nullptr);
            return nullptr;
        }
        return found[0];
    }

    template <typename T, typename... A> std::vector<const T *> select(const std::string &tail, const A &...args) {
        Stmt &s = db_.prepare(std::string("SELECT ") + T::COLUMNS + " FROM \"" + T::TABLE + "\" " + tail);
        s.bind_all(args...);
        std::vector<const T *> out;
        while (s.step()) {
            auto row = std::make_unique<T>();
            row->read(s, 0);
            if constexpr (requires { row->pk(); }) {
                auto &map = rows<T>();
                auto it = map.find(row->pk());
                if (it == map.end() || !it->second) {
                    out.push_back(row.get());
                    map[row->pk()] = std::move(row);
                } else
                    out.push_back(it->second.get());
            } else {
                out.push_back(row.get());
                transient_.push_back(std::shared_ptr<void>(std::move(row)));
            }
        }
        s.reset();
        return out;
    }

    template <typename T, typename... A> const T *first(const std::string &tail, const A &...args) {
        std::vector<const T *> found = select<T>(tail + " LIMIT 1", args...);
        return found.empty() ? nullptr : found[0];
    }

    template <typename T, typename... A> int64_t count(const std::string &tail, const A &...args) {
        return scalar_int(std::string("SELECT COUNT(*) FROM \"") + T::TABLE + "\" " + tail, args...).value_or(0);
    }

    template <typename... A> std::optional<int64_t> scalar_int(const std::string &sql, const A &...args) {
        return db_.scalar_int(sql, args...);
    }

    Stmt &query(const std::string &sql) { return db_.prepare(sql); }

    void end_request() { transient_.clear(); }

private:
    struct MapBase {
        virtual ~MapBase() = default;
    };
    template <typename T> struct Map final : MapBase {
        std::map<typename T::Key, std::unique_ptr<T>> rows;
    };
    template <typename T> std::map<typename T::Key, std::unique_ptr<T>> &rows() {
        auto &slot = maps_[std::type_index(typeid(T))];
        if (!slot) slot = std::make_unique<Map<T>>();
        return static_cast<Map<T> *>(slot.get())->rows;
    }

    Database &db_;
    std::map<std::type_index, std::unique_ptr<MapBase>> maps_;
    std::vector<std::shared_ptr<void>> transient_;
};

}  // namespace npps::db
