// npps4/game/stamp.py
#include "app/idol.hpp"

namespace npps::game::stamp {

static const char *const STUB_DATA =
    R"J({"owning_stamp_ids":[1001,1002,1003,1004,1005,1006,1007,1008,1009,1,2,19,3,4,20,5,6,21,7,8,22,9,10,23,11,12,24)J"
    R"J(,13,14,25,15,16,26,17,18,27,201,101,102,119,103,104,120,105,106,121,107,108,122,109,110,123,111,112,124,113,11)J"
    R"J(4,125,115,116,126,117,118,127,301],"stamp_setting":[{"stamp_type":1,"setting_list":[{"stamp_setting_id":1,"mai)J"
    R"J(n_flag":1,"stamp_list":[{"position":1,"stamp_id":2},{"position":2,"stamp_id":101},{"position":3,"stamp_id":4},)J"
    R"J({"position":4,"stamp_id":103},{"position":5,"stamp_id":5},{"position":6,"stamp_id":105},{"position":7,"stamp_i)J"
    R"J(d":7},{"position":8,"stamp_id":108},{"position":9,"stamp_id":10},{"position":10,"stamp_id":110},{"position":11)J"
    R"J(,"stamp_id":12},{"position":12,"stamp_id":112},{"position":13,"stamp_id":13},{"position":14,"stamp_id":114},{")J"
    R"J(position":15,"stamp_id":16},{"position":16,"stamp_id":116},{"position":17,"stamp_id":17},{"position":18,"stamp)J"
    R"J(_id":117}]},{"stamp_setting_id":2,"main_flag":0,"stamp_list":[{"position":1,"stamp_id":2},{"position":2,"stamp)J"
    R"J(_id":101},{"position":3,"stamp_id":4},{"position":4,"stamp_id":103},{"position":5,"stamp_id":5},{"position":6,)J"
    R"J("stamp_id":105},{"position":7,"stamp_id":7},{"position":8,"stamp_id":108},{"position":9,"stamp_id":10},{"posit)J"
    R"J(ion":10,"stamp_id":110},{"position":11,"stamp_id":12},{"position":12,"stamp_id":112},{"position":13,"stamp_id")J"
    R"J(:13},{"position":14,"stamp_id":114},{"position":15,"stamp_id":16},{"position":16,"stamp_id":116},{"position":1)J"
    R"J(7,"stamp_id":17},{"position":18,"stamp_id":117}]},{"stamp_setting_id":3,"main_flag":0,"stamp_list":[{"position)J"
    R"J(":1,"stamp_id":2},{"position":2,"stamp_id":101},{"position":3,"stamp_id":4},{"position":4,"stamp_id":103},{"po)J"
    R"J(sition":5,"stamp_id":5},{"position":6,"stamp_id":105},{"position":7,"stamp_id":7},{"position":8,"stamp_id":108)J"
    R"J(},{"position":9,"stamp_id":10},{"position":10,"stamp_id":110},{"position":11,"stamp_id":12},{"position":12,"st)J"
    R"J(amp_id":112},{"position":13,"stamp_id":13},{"position":14,"stamp_id":114},{"position":15,"stamp_id":16},{"posi)J"
    R"J(tion":16,"stamp_id":116},{"position":17,"stamp_id":17},{"position":18,"stamp_id":117}]},{"stamp_setting_id":4,)J"
    R"J("main_flag":0,"stamp_list":[{"position":1,"stamp_id":2},{"position":2,"stamp_id":101},{"position":3,"stamp_id")J"
    R"J(:4},{"position":4,"stamp_id":103},{"position":5,"stamp_id":5},{"position":6,"stamp_id":105},{"position":7,"sta)J"
    R"J(mp_id":7},{"position":8,"stamp_id":108},{"position":9,"stamp_id":10},{"position":10,"stamp_id":110},{"position)J"
    R"J(":11,"stamp_id":12},{"position":12,"stamp_id":112},{"position":13,"stamp_id":13},{"position":14,"stamp_id":114)J"
    R"J(},{"position":15,"stamp_id":16},{"position":16,"stamp_id":116},{"position":17,"stamp_id":17},{"position":18,"s)J"
    R"J(tamp_id":117}]},{"stamp_setting_id":5,"main_flag":0,"stamp_list":[{"position":1,"stamp_id":2},{"position":2,"s)J"
    R"J(tamp_id":101},{"position":3,"stamp_id":4},{"position":4,"stamp_id":103},{"position":5,"stamp_id":5},{"position)J"
    R"J(":6,"stamp_id":105},{"position":7,"stamp_id":7},{"position":8,"stamp_id":108},{"position":9,"stamp_id":10},{"p)J"
    R"J(osition":10,"stamp_id":110},{"position":11,"stamp_id":12},{"position":12,"stamp_id":112},{"position":13,"stamp)J"
    R"J(_id":13},{"position":14,"stamp_id":114},{"position":15,"stamp_id":16},{"position":16,"stamp_id":116},{"positio)J"
    R"J(n":17,"stamp_id":17},{"position":18,"stamp_id":117}]}]},{"stamp_type":2,"setting_list":[{"stamp_setting_id":1,)J"
    R"J("main_flag":1,"stamp_list":[{"position":1,"stamp_id":1001},{"position":2,"stamp_id":1002},{"position":3,"stamp)J"
    R"J(_id":1003},{"position":4,"stamp_id":1004},{"position":5,"stamp_id":1005},{"position":6,"stamp_id":1006},{"posi)J"
    R"J(tion":7,"stamp_id":1007},{"position":8,"stamp_id":1008},{"position":9,"stamp_id":1009}]},{"stamp_setting_id":2)J"
    R"J(,"main_flag":0,"stamp_list":[{"position":1,"stamp_id":1001},{"position":2,"stamp_id":1002},{"position":3,"stam)J"
    R"J(p_id":1003},{"position":4,"stamp_id":1004},{"position":5,"stamp_id":1005},{"position":6,"stamp_id":1006},{"pos)J"
    R"J(ition":7,"stamp_id":1007},{"position":8,"stamp_id":1008},{"position":9,"stamp_id":1009}]},{"stamp_setting_id":)J"
    R"J(3,"main_flag":0,"stamp_list":[{"position":1,"stamp_id":1001},{"position":2,"stamp_id":1002},{"position":3,"sta)J"
    R"J(mp_id":1003},{"position":4,"stamp_id":1004},{"position":5,"stamp_id":1005},{"position":6,"stamp_id":1006},{"po)J"
    R"J(sition":7,"stamp_id":1007},{"position":8,"stamp_id":1008},{"position":9,"stamp_id":1009}]},{"stamp_setting_id")J"
    R"J(:4,"main_flag":0,"stamp_list":[{"position":1,"stamp_id":1001},{"position":2,"stamp_id":1002},{"position":3,"st)J"
    R"J(amp_id":1003},{"position":4,"stamp_id":1004},{"position":5,"stamp_id":1005},{"position":6,"stamp_id":1006},{"p)J"
    R"J(osition":7,"stamp_id":1007},{"position":8,"stamp_id":1008},{"position":9,"stamp_id":1009}]},{"stamp_setting_id)J"
    R"J(":5,"main_flag":0,"stamp_list":[{"position":1,"stamp_id":1001},{"position":2,"stamp_id":1002},{"position":3,"s)J"
    R"J(tamp_id":1003},{"position":4,"stamp_id":1004},{"position":5,"stamp_id":1005},{"position":6,"stamp_id":1006},{")J"
    R"J(position":7,"stamp_id":1007},{"position":8,"stamp_id":1008},{"position":9,"stamp_id":1009}]}]}]})J";

static StampInfoResponse stamp_stampinfo(Context &context) {
    // TODO
    util::log("STUB stamp/stampInfo %s", context.raw_request_data.c_str());
    return model::parse<StampInfoResponse>(Json::parse(STUB_DATA));
}
NPPS_ENDPOINT("stamp", "stampInfo", User, stamp_stampinfo);

}  // namespace npps::game::stamp
