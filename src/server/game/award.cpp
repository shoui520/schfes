// npps4/game/award.py
#include "app/idol.hpp"
#include "system/award.hpp"
#include "system/user.hpp"

namespace npps::game::award {

static AwardInfoResponse award_awardinfo(Context &context) {
    db::main::User *current_user = system::user::get_current(context);
    std::vector<db::main::Award *> awards = system::award::get_awards(context, *current_user);
    std::vector<AwardInfo> award_info;
    for (const db::main::Award *aw : awards)
        award_info.push_back(AwardInfo::make(aw->award_id, current_user->active_award == aw->award_id,
                                             util::timestamp_to_datetime(aw->insert_date)));

    return AwardInfoResponse::make(award_info);
}
NPPS_ENDPOINT("award", "awardInfo", User, award_awardinfo);

static void award_set(Context &context, const AwardSetRequest &request) {
    db::main::User *current_user = system::user::get_current(context);
    if (system::award::has_award(context, *current_user, request.award_id)) {
        current_user->active_award = request.award_id;
        return;
    }

    throw error::IdolError(200, 720, "No such award");
}
NPPS_ENDPOINT("award", "set", User, award_set);

}  // namespace npps::game::award
