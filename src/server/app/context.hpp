#pragma once
// npps4/idol/session.py contexts. One Context type covers
// BasicSchoolIdolContext, SchoolIdolParams, SchoolIdolAuthParams and
// SchoolIdolUserParams; `kind` says which one an endpoint asked for.
#include "app/model.hpp"
#include "core/http.hpp"
#include "core/json.hpp"
#include "core/orm.hpp"
#include "db/game_tables.hpp"
#include "db/main_tables.hpp"

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace npps {

struct TokenData {
    Bytes client_key, server_key;
    int64_t user_id = 0;
};

// context.db: the main database session plus the read-only master data.
struct Databases {
    db::Session &main;
    db::ReadSession &game_mater, &item, &live, &unit, &achievement, &effort, &subscenario, &museum, &scenario,
        &exchange;
};

// Process-wide database handles; opened by app::open_databases().
Databases &databases();

class Context {
public:
    enum Kind { Basic, Params, Auth, User };

    explicit Context(Language lang = Language::jp);
    virtual ~Context();
    Context(const Context &) = delete;

    Kind kind = Basic;
    Language lang;
    Databases &db;

    // Request state (kind >= Params).
    const http::Request *request = nullptr;
    std::pair<int, int> client_version{0, 0};
    int64_t nonce = 0;
    std::optional<std::string> token_text;
    std::optional<TokenData> token;
    int64_t timestamp = 0;
    PlatformType platform = PlatformType::Android;
    std::optional<std::string> x_message_code;
    std::string raw_request_data;

    bool is_lang_jp() const { return lang == Language::jp; }
    const std::string &get_text(const std::string &text_jp, const std::optional<std::string> &text_en) const {
        return is_lang_jp() || !text_en ? text_jp : *text_en;
    }
    std::optional<std::string> get_text(const std::optional<std::string> &text_jp,
                                        const std::optional<std::string> &text_en) const {
        if (is_lang_jp()) return text_jp;
        return text_en ? text_en : text_jp;
    }

    // Per-request memo (context.get_cache/set_cache and @context_cacheable):
    //   auto &m = context.memo<std::map<int64_t, Json>>("unit_info");
    template <typename M> M &memo(const char *key) {
        std::any &slot = cache_[key];
        if (!slot.has_value()) slot = M();
        return std::any_cast<M &>(slot);
    }
    void clear_memo() { cache_.clear(); }

    // Background tasks run after the response is sent (context.add_task).
    bool support_background_task() const { return kind != Basic; }
    void add_task(std::function<void()> task) { tasks_.push_back(std::move(task)); }
    std::vector<std::function<void()>> take_tasks() { return std::move(tasks_); }

    // `async with context`: commit on success, roll back on error.
    void commit();
    void rollback();

private:
    std::map<std::string, std::any> cache_;
    std::vector<std::function<void()>> tasks_;
};

namespace db {
// npps4/db/__init__.py. The server has no release keys (the "none" download
// backend), so rows encrypted with one are unavailable, as in NPPS4.
template <typename T> const T *decrypt_row(const T *row) {
    if constexpr (requires { row->_encryption_release_id; })
        if (row && row->_encryption_release_id) return nullptr;
    return row;
}
template <typename T> const T *get_decrypted_row(ReadSession &session, const typename T::Key &id) {
    return decrypt_row(session.get<T>(id));
}
}  // namespace db

}  // namespace npps
