#include "context.hpp"

namespace npps {

Context::Context(Language lang) : lang(lang), db(databases()) {}

Context::~Context() {
    try {
        if (db.main.database().in_transaction()) rollback();
    } catch (...) {
    }
}

void Context::commit() {
    cache_.clear();
    try {
        db.main.commit();
    } catch (...) {
        rollback();
        throw;
    }
    db.main.clear();
    for (db::ReadSession *s : {&db.game_mater, &db.item, &db.live, &db.unit, &db.achievement, &db.effort,
                               &db.subscenario, &db.museum, &db.scenario, &db.exchange})
        s->end_request();
}

void Context::rollback() {
    cache_.clear();
    db.main.rollback();
    for (db::ReadSession *s : {&db.game_mater, &db.item, &db.live, &db.unit, &db.achievement, &db.effort,
                               &db.subscenario, &db.museum, &db.scenario, &db.exchange})
        s->end_request();
}

}  // namespace npps
