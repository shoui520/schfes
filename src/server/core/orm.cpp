#include "orm.hpp"

namespace npps::db {

void Session::begin_if_needed() {
    if (!db_.in_transaction()) db_.begin();
}

// Unit of work: inserts in add() order, then updates, then deletes.
void Session::flush() {
    if (flushing_) return;
    bool work = !pending_.empty() || !deleted_.empty();
    if (!work)
        for (TrackedBase *t : loaded_)
            if (t->dirty()) {
                work = true;
                break;
            }
    if (!work) return;
    flushing_ = true;
    try {
        begin_if_needed();
        std::vector<TrackedBase *> pending;
        pending.swap(pending_);
        for (TrackedBase *t : pending)
            if (t->state == TrackedBase::Pending) t->insert(db_, *this);
        for (TrackedBase *t : loaded_)
            if (t->dirty()) t->update(db_);
        std::vector<TrackedBase *> deleted;
        deleted.swap(deleted_);
        for (TrackedBase *t : deleted) t->remove(db_);
    } catch (...) {
        flushing_ = false;
        throw;
    }
    flushing_ = false;
}

void Session::commit() {
    flush();
    if (db_.in_transaction()) db_.commit();
}

void Session::rollback() {
    try {
        db_.rollback();
    } catch (...) {
    }
    clear();
}

void Session::clear() {
    maps_.clear();
    loaded_.clear();
    pending_.clear();
    deleted_.clear();
    owned_.clear();
}

}  // namespace npps::db
