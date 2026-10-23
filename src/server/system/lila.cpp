// npps4/system/lila.py
#include "system/lila.hpp"

#include "app/config.hpp"
#include "core/crypto.hpp"
#include "system/achievement.hpp"
#include "system/advanced.hpp"
#include "system/award.hpp"
#include "system/background.hpp"
#include "system/const.hpp"
#include "system/exchange.hpp"
#include "system/item.hpp"
#include "system/lbonus.hpp"
#include "system/live.hpp"
#include "system/museum.hpp"
#include "system/reward.hpp"
#include "system/scenario.hpp"
#include "system/subscenario.hpp"
#include "system/unit.hpp"
#include "system/user.hpp"

#include <zlib.h>

#include <map>

namespace npps::system::lila {

namespace {

// pydantic model_dump_json(): compact separators and non-ASCII text left as is.
void dump_pydantic(const Json &j, std::string &out) {
    switch (j.type()) {
    case Json::String: {
        out.push_back('"');
        for (unsigned char c : j.as_string()) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) out += util::format("\\u%04x", c);
                else out.push_back((char)c);
            }
        }
        out.push_back('"');
        break;
    }
    case Json::Array: {
        out.push_back('[');
        bool first = true;
        for (const Json &e : j.elements()) {
            if (!first) out.push_back(',');
            first = false;
            dump_pydantic(e, out);
        }
        out.push_back(']');
        break;
    }
    case Json::Object: {
        out.push_back('{');
        bool first = true;
        for (const auto &m : j.members()) {
            if (!first) out.push_back(',');
            first = false;
            dump_pydantic(Json(m.first), out);
            out.push_back(':');
            dump_pydantic(m.second, out);
        }
        out.push_back('}');
        break;
    }
    default: j.dump_to(out); break;
    }
}

// hmac.new(key, msg, hashlib.sha256).digest()
Bytes hmac_sha256(std::string_view key, std::string_view message) {
    constexpr size_t BLOCK = 64;
    Bytes k(key);
    if (k.size() > BLOCK) k = crypto::sha256(k);
    k.resize(BLOCK, '\0');
    Bytes ipad(BLOCK, '\0'), opad(BLOCK, '\0');
    for (size_t i = 0; i < BLOCK; i++) {
        ipad[i] = (char)(k[i] ^ 0x36);
        opad[i] = (char)(k[i] ^ 0x5c);
    }
    return crypto::sha256(opad + crypto::sha256(ipad + Bytes(message)));
}

// zlib.compress(data, 9, 31)
Bytes gzip_compress(const Bytes &data) {
    z_stream zs{};
    if (deflateInit2(&zs, 9, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("deflateInit2 failed");
    Bytes out(deflateBound(&zs, (uLong)data.size()) + 32, '\0');
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = (uInt)data.size();
    zs.next_out = (Bytef *)out.data();
    zs.avail_out = (uInt)out.size();
    int rc = deflate(&zs, Z_FINISH);
    out.resize(zs.total_out);
    deflateEnd(&zs);
    if (rc != Z_STREAM_END) throw std::runtime_error("deflate failed");
    return out;
}

// zlib.decompress(data, 31)
Bytes gzip_decompress(std::string_view data) {
    z_stream zs{};
    if (inflateInit2(&zs, 31) != Z_OK) throw std::runtime_error("inflateInit2 failed");
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = (uInt)data.size();
    Bytes out;
    char buf[16384];
    int rc;
    do {
        zs.next_out = (Bytef *)buf;
        zs.avail_out = sizeof(buf);
        rc = inflate(&zs, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) {
            inflateEnd(&zs);
            throw std::runtime_error("zlib.error: Error -3 while decompressing data");
        }
        out.append(buf, sizeof(buf) - zs.avail_out);
    } while (rc != Z_STREAM_END && (zs.avail_in > 0 || zs.avail_out == 0));
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) throw std::runtime_error("zlib.error: Error -5 while decompressing data");
    return out;
}

bool already_expired(const db::main::Incentive &pbox, int64_t time) {
    // The unit-rarity case of NPPS4 requires expire_date to be None, which the column never is.
    if (pbox.expire_date == 0) return false;
    return time > pbox.expire_date;
}

std::string two_digits(int64_t v, int width) {
    std::string s = std::to_string(v);
    if ((int)s.size() < width) s.insert(0, width - s.size(), '0');
    return s;
}

}  // namespace

std::tuple<Bytes, Bytes> export_user(Context &context, db::main::User &target, const std::optional<Bytes> &secret_key,
                                     bool nullify_credentials) {
    /*
    Export user data to JSON which can be imported back by "compatible" implementation.

    Note that it returns 2 values, the zlib-compressed JSON bytes and the signature of the **uncompressed** data.
    For security reasons, signature is required for data importing unless server configured otherwise.

    The 1st return value is bytes consist of 16-byte salt + zlib-compressed JSON data.
    The 2nd return value is HMAC-SHA256 hash of the salt + uncompressed JSON data with the HMAC key set to server key.
    */
    Bytes key = secret_key ? *secret_key : Bytes(config::get_secret_key());

    UserData user_data;
    if (!nullify_credentials) {
        user_data.key = target.key;
        user_data.passwd = target.passwd;
        user_data.transfer_sha1 = target.transfer_sha1;
    }
    user_data.name = target.name;
    user_data.bio = target.bio;
    user_data.exp = target.exp;
    user_data.coin = target.game_coin;
    user_data.sns_coin = {target.free_sns_coin, target.paid_sns_coin};
    user_data.friend_pts = target.social_point;
    user_data.unit_max = target.unit_max;
    user_data.waiting_unit_max = target.waiting_unit_max;
    user_data.energy_max = target.energy_max;
    user_data.energy_full_time = target.energy_full_time;
    user_data.license_live_energy_recoverly_time = target.license_live_energy_recoverly_time;
    user_data.energy_full_need_time = target.energy_full_need_time;
    user_data.over_max_energy = target.over_max_energy;
    user_data.training_energy = target.training_energy;
    user_data.training_energy_max = target.training_energy_max;
    user_data.friend_max = target.friend_max;
    user_data.tutorial_state = target.tutorial_state;
    user_data.active_deck_index = target.active_deck_index;
    user_data.active_background = target.active_background;
    user_data.active_award = target.active_award;
    user_data.live_effort_point_box_spec_id = target.live_effort_point_box_spec_id;
    user_data.limited_effort_event_id = target.limited_effort_event_id;
    user_data.current_live_effort_point = target.current_live_effort_point;
    user_data.current_limited_effort_point = target.current_limited_effort_point;

    // Backgrounds
    std::vector<int64_t> background_list;
    for (db::main::Background *bkg : background::get_backgrounds(context, target))
        background_list.push_back(bkg->background_id);

    // Awards
    std::vector<int64_t> award_list;
    for (db::main::Award *aw : award::get_awards(context, target)) award_list.push_back(aw->award_id);

    // Iterate all units
    std::vector<UnitData> unit_data_list;
    std::map<int64_t, int64_t> unit_owning_user_id_lookup;  // [unit_owning_user_id, index+1]
    for (db::main::Unit *unit_data : unit::get_all_units(context, target)) {
        UnitData unit_data_serialized;
        unit_data_serialized.unit_id = unit_data->unit_id;
        // bits: 0 = active, 1 = fav. flag, 2 = signed, 3-4 = rank, 5-6 = display rank
        unit_data_serialized.flags = (int64_t)unit_data->active | ((int64_t)unit_data->favorite_flag << 1) |
                                     ((int64_t)unit_data->is_signed << 2) | (unit_data->rank << 3) |
                                     (unit_data->display_rank << 5);
        unit_data_serialized.exp = unit_data->exp;
        unit_data_serialized.skill_exp = unit_data->skill_exp;
        unit_data_serialized.max_level = unit_data->max_level;
        unit_data_serialized.love = unit_data->love;
        unit_data_serialized.level_limit_id = unit_data->level_limit_id;
        unit_data_serialized.removable_skill_capacity = unit_data->unit_removable_skill_capacity;
        unit_data_list.push_back(std::move(unit_data_serialized));
        unit_owning_user_id_lookup[unit_data->id] = (int64_t)unit_data_list.size();
    }
    if (target.center_unit_owning_user_id > 0)
        user_data.center_unit_owning_user_id = unit_owning_user_id_lookup.at(target.center_unit_owning_user_id);

    // Supporter unit
    std::vector<CommonItemData> supp_unit_list;
    for (const auto &info : unit::get_all_supporter_unit(context, target))
        supp_unit_list.push_back(CommonItemData::make(std::get<0>(info), std::get<1>(info)));

    // Deck
    std::vector<DeckData> deck_data_list;
    for (const auto &deck : unit::get_all_deck_simple(context, target)) {
        DeckData d;
        d.name = std::get<1>(deck);
        d.index = std::get<0>(deck);
        for (int64_t unit_owning_user_id : std::get<2>(deck))
            d.units.push_back(unit_owning_user_id == 0 ? 0 : unit_owning_user_id_lookup.at(unit_owning_user_id));
        deck_data_list.push_back(std::move(d));
    }

    // SIS/Removable Skill
    unit_model::RemovableSkillInfoResponse removable_skill_data = unit::get_removable_skill_info_request(context, target);
    std::vector<CommonItemData> removable_skill_list;
    for (const auto &info : removable_skill_data.owning_info)
        removable_skill_list.push_back(CommonItemData::make(info.unit_removable_skill_id, info.total_amount));
    if (removable_skill_data.equipment_info.is_object()) {
        for (const auto &entry : removable_skill_data.equipment_info.members()) {
            const Json &equip_info = entry.second;
            UnitData &unit_data = unit_data_list.at(
                (size_t)unit_owning_user_id_lookup.at(equip_info.at("unit_owning_user_id").as_int()) - 1);
            unit_data.removable_skills.clear();
            for (const Json &d : equip_info.at("detail").elements())
                unit_data.removable_skills.push_back(d.at("unit_removable_skill_id").as_int());
        }
    }

    // Achievement
    std::vector<AchievementData> achievement_list;
    for (db::main::Achievement *ach : achievement::get_achievements(context, target, std::nullopt)) {
        AchievementData a;
        a.achievement_id = ach->achievement_id;
        a.count = ach->count;
        // bits: 0 = is_accomplished, 1 = is_reward_claimed, 2 = is_new
        a.flags = (int64_t)ach->is_accomplished | ((int64_t)ach->is_reward_claimed << 1) | ((int64_t)ach->is_new << 2);
        a.reset_value = ach->reset_value;
        achievement_list.push_back(a);
    }

    // Login bonus
    std::vector<std::string> login_bonus_list;
    for (const auto &lb : lbonus::all_login_bonus(context, target))
        login_bonus_list.push_back(two_digits(std::get<2>(lb), 4) + two_digits(std::get<1>(lb), 2) +
                                   two_digits(std::get<0>(lb), 2));

    // Present box
    int64_t time = util::time();
    std::vector<PresentBoxData> present_box_list;
    for (db::main::Incentive *pbox : reward::get_presentbox_simple(context, target)) {
        if (!already_expired(*pbox, time)) continue;
        PresentBoxData p;
        p.add_type = (const_::ADD_TYPE)pbox->add_type;
        p.item_id = pbox->item_id;
        p.amount = pbox->amount;
        p.message_jp = pbox->message_jp;
        p.message_en = pbox->message_en;
        p.expire = pbox->expire_date;
        p.extra_data = pbox->extra_data ? Json::parse(*pbox->extra_data) : Json();
        present_box_list.push_back(std::move(p));
    }

    // Scenario
    std::vector<int64_t> scenario_encoded_list;
    for (db::main::Scenario *sc : scenario::get_all(context, target))
        scenario_encoded_list.push_back(sc->scenario_id * (sc->completed ? 1 : -1));

    // Subscenario
    std::vector<int64_t> subscenario_encoded_list;
    for (db::main::SubScenario *sc : subscenario::get_all(context, target))
        subscenario_encoded_list.push_back(sc->subscenario_id * (sc->completed ? 1 : -1));

    // Museum
    museum::MuseumInfoData museum_data = museum::get_museum_info_data(context, target);

    // Live clear
    std::vector<LiveClearData> live_clear_data;
    for (db::main::LiveClear *lc : live::get_all_live_clear_data(context, target))
        live_clear_data.push_back(
            LiveClearData::make(lc->live_difficulty_id, lc->hi_score, lc->hi_combo_cnt, lc->clear_cnt));

    // Regular items
    auto [items_full_0, items_full_1, items_full_2] = item::get_item_list(context, target);
    auto to_common = [](const std::vector<common::ItemCount> &list) {
        std::vector<CommonItemData> out;
        for (const auto &info : list) out.push_back(CommonItemData::make(info.item_id, info.amount));
        return out;
    };
    std::vector<CommonItemData> general_item_list = to_common(items_full_0);
    std::vector<CommonItemData> buff_item_list = to_common(items_full_1);
    std::vector<CommonItemData> reinforce_item_list = to_common(items_full_2);

    // Recovery items
    std::vector<CommonItemData> recovery_item_list = to_common(item::get_recovery_items(context, target));

    // Exchange
    std::vector<CommonItemData> exchange_points;
    for (db::main::ExchangePointItem *info : exchange::get_exchange_point_list(context, target))
        if (info->amount > 0) exchange_points.push_back(CommonItemData::make(info->exchange_point_id, info->amount));

    // Normal live unlock
    std::vector<int64_t> normal_live_unlock = live::get_all_normal_live_unlock(context, target);

    AccountData account_data;
    account_data.user = std::move(user_data);
    account_data.background = std::move(background_list);
    account_data.award = std::move(award_list);
    account_data.unit = std::move(unit_data_list);
    account_data.supp_unit = std::move(supp_unit_list);
    account_data.sis = std::move(removable_skill_list);
    account_data.deck = std::move(deck_data_list);
    account_data.achievement = std::move(achievement_list);
    account_data.login_bonus = std::move(login_bonus_list);
    account_data.present_box = std::move(present_box_list);
    account_data.scenario = std::move(scenario_encoded_list);
    account_data.subscenario = std::move(subscenario_encoded_list);
    account_data.museum = museum_data.contents_id_list;
    account_data.live_clear = std::move(live_clear_data);
    account_data.normal_live_unlock = std::move(normal_live_unlock);
    account_data.items = std::move(general_item_list);
    account_data.buff_items = std::move(buff_item_list);
    account_data.reinforce_items = std::move(reinforce_item_list);
    account_data.recovery_items = std::move(recovery_item_list);
    account_data.exchange = std::move(exchange_points);

    return export_account_data(account_data, key);
}

std::tuple<Bytes, Bytes> export_account_data(const AccountData &account_data, const std::optional<Bytes> &secret_key) {
    Bytes key = secret_key ? *secret_key : Bytes(config::get_secret_key());

    Bytes json_encoded;
    dump_pydantic(account_data.to_json(), json_encoded);
    Bytes salt = util::randbytes(16);
    Bytes digest = hmac_sha256(key, salt + json_encoded);

    Bytes result = salt + gzip_compress(json_encoded);
    return {std::move(result), std::move(digest)};
}

AccountData extract_serialized_data(const Bytes &serialized_data, const std::optional<Bytes> &signature,
                                    const std::optional<Bytes> &secret_key) {
    Bytes salt = serialized_data.substr(0, 16);
    Bytes json_encoded = gzip_decompress(serialized_data.size() > 16 ? std::string_view(serialized_data).substr(16)
                                                                       : std::string_view());

    if (signature) {
        Bytes key = secret_key ? *secret_key : Bytes(config::get_secret_key());
        if (hmac_sha256(key, salt + json_encoded) != *signature) throw BadSignature();
    }

    return model::parse<AccountData>(Json::parse(json_encoded));
}

db::main::User *import_user(Context &context, const AccountData &serialized_data) {
    db::main::User *target = user::create(context, std::nullopt, std::nullopt);
    const UserData &ud = serialized_data.user;
    target->key = ud.key;
    target->passwd = ud.passwd;
    target->transfer_sha1 = ud.transfer_sha1;
    target->name = ud.name;
    target->bio = ud.bio;
    target->exp = ud.exp;
    target->game_coin = ud.coin;
    if (ud.sns_coin.size() != 2) throw std::invalid_argument("sns_coin must contain exactly 2 values");
    target->free_sns_coin = ud.sns_coin[0];
    target->paid_sns_coin = ud.sns_coin[1];
    target->social_point = ud.friend_pts;
    target->unit_max = ud.unit_max;
    target->waiting_unit_max = ud.waiting_unit_max;
    target->energy_max = ud.energy_max;
    target->energy_full_time = ud.energy_full_time;
    target->license_live_energy_recoverly_time = ud.license_live_energy_recoverly_time;
    target->energy_full_need_time = ud.energy_full_need_time;
    target->over_max_energy = ud.over_max_energy;
    target->training_energy = ud.training_energy;
    target->training_energy_max = ud.training_energy_max;
    target->friend_max = ud.friend_max;
    target->tutorial_state = ud.tutorial_state;
    target->active_deck_index = ud.active_deck_index;
    target->active_background = ud.active_background;
    target->active_award = ud.active_award;
    target->live_effort_point_box_spec_id = ud.live_effort_point_box_spec_id;
    target->limited_effort_event_id = ud.limited_effort_event_id;
    target->current_live_effort_point = ud.current_live_effort_point;
    target->current_limited_effort_point = ud.current_limited_effort_point;
    user::add_exp(context, *target, 0);  // To enforce level up

    // Backgrounds
    for (int64_t bg : serialized_data.background) {
        if (!background::has_background(context, *target, bg))
            background::unlock_background(context, *target, bg, target->active_background == bg);
    }

    // Awards
    for (int64_t aw : serialized_data.award) {
        if (!award::has_award(context, *target, aw)) award::unlock_award(context, *target, aw, target->active_award == aw);
    }

    // Removable skill (note: must be done first before adding units)
    for (const CommonItemData &d : serialized_data.sis)
        if (has_quantity(d)) unit::add_unit_removable_skill(context, *target, d.id, d.amount);

    // Units
    std::map<int64_t, int64_t> reverse_unit_owning_user_id_lookup;
    int64_t index = 1;
    for (const UnitData &unit_sdata : serialized_data.unit) {
        unit_model::UnitExtraData extra;
        extra.exp = unit_sdata.exp;
        extra.rank = (unit_sdata.flags >> 3) & 3;
        extra.love = unit_sdata.love;
        extra.skill_exp = unit_sdata.skill_exp;
        extra.unit_removable_skill_capacity = unit_sdata.removable_skill_capacity;
        extra.display_rank = (unit_sdata.flags >> 5) & 3;
        extra.is_signed = (unit_sdata.flags & 4) != 0;
        extra.removable_skill_ids = unit_sdata.removable_skills;
        db::main::Unit *unit_data =
            unit::add_unit_simple(context, *target, unit_sdata.unit_id, (unit_sdata.flags & 1) != 0, extra);
        if (unit_data == nullptr) throw std::logic_error("assertion failed");
        unit_data->favorite_flag = (unit_sdata.flags & 2) != 0;
        reverse_unit_owning_user_id_lookup[index] = unit_data->id;

        for (int64_t removable_skill_id : unit_sdata.removable_skills)
            unit::attach_unit_removable_skill(context, *unit_data, removable_skill_id);
        index++;
    }
    if (ud.center_unit_owning_user_id > 0) {
        // Reverse
        target->center_unit_owning_user_id = reverse_unit_owning_user_id_lookup.at(ud.center_unit_owning_user_id);
    }

    // Support Unit
    for (const CommonItemData &d : serialized_data.supp_unit)
        if (has_quantity(d)) unit::add_supporter_unit(context, *target, d.id, d.amount);

    // Deck
    for (const DeckData &deck_sdata : serialized_data.deck) {
        if (deck_sdata.units.size() != 9) throw std::logic_error("assertion failed");
        auto loaded = unit::load_unit_deck(context, *target, deck_sdata.index, true);
        db::main::UnitDeck *deck_data = std::get<0>(*loaded);
        deck_data->name = deck_sdata.name;
        std::vector<int64_t> ids;
        for (int64_t i : deck_sdata.units) {
            auto it = reverse_unit_owning_user_id_lookup.find(i);
            ids.push_back(it == reverse_unit_owning_user_id_lookup.end() ? 0 : it->second);
        }
        unit::save_unit_deck(context, *target, *deck_data, ids);
    }

    // Login Bonus
    for (const std::string &datestr : serialized_data.login_bonus) {
        lbonus::mark_login_bonus(context, *target, std::stoll(datestr.substr(0, 4)),
                                 std::stoll(datestr.substr(4, 2)), std::stoll(datestr.substr(6, 2)));
    }

    // Present Box
    int64_t time = util::time();
    for (const PresentBoxData &pbox : serialized_data.present_box) {
        if (pbox.expire != 0 && pbox.expire >= time) {
            auto deserialized_item = advanced::deserialize_item_data(context, pbox);
            reward::add_item(context, *target, deserialized_item, pbox.message_jp, pbox.message_en, pbox.expire);
        }
    }

    // Scenario
    for (int64_t scenario_sid : serialized_data.scenario) {
        int64_t scenario_id = scenario_sid < 0 ? -scenario_sid : scenario_sid;
        db::main::Scenario *scenario_data = scenario::get(context, *target, scenario_id);
        if (scenario_data == nullptr) {
            if (!scenario::unlock(context, *target, scenario_id)) throw std::logic_error("assertion failed");
            scenario_data = scenario::get(context, *target, scenario_id);
            if (scenario_data == nullptr) throw std::logic_error("assertion failed");
        }
        scenario_data->completed = scenario_sid > 0;
    }

    // Subscenario
    for (int64_t subscenario_sid : serialized_data.subscenario) {
        int64_t subscenario_id = subscenario_sid < 0 ? -subscenario_sid : subscenario_sid;
        db::main::SubScenario *subscenario_data = subscenario::get(context, *target, subscenario_id);
        if (subscenario_data == nullptr) {
            if (!subscenario::unlock(context, *target, subscenario_id)) throw std::logic_error("assertion failed");
            subscenario_data = subscenario::get(context, *target, subscenario_id);
            if (subscenario_data == nullptr) throw std::logic_error("assertion failed");
        }
        subscenario_data->completed = subscenario_sid > 0;
    }

    // Museum
    for (int64_t museum_content_id : serialized_data.museum) museum::unlock(context, *target, museum_content_id);

    // Normal Live unlock (done this first before live clear)
    for (int64_t live_track_id : serialized_data.normal_live_unlock)
        live::unlock_normal_live(context, *target, live_track_id);

    // Live Clear tracking
    for (const LiveClearData &live_clear_sdata : serialized_data.live_clear) {
        db::main::LiveClear *live_clear_data =
            live::get_live_clear_data(context, *target, live_clear_sdata.live_difficulty_id, true);
        live_clear_data->hi_score = live_clear_sdata.hi_score;
        live_clear_data->hi_combo_cnt = live_clear_sdata.hi_combo_cnt;
        live_clear_data->clear_cnt = live_clear_sdata.clear_cnt;
    }

    // Items
    // TODO: Is this itertools.chain correct?
    for (const auto *list : {&serialized_data.items, &serialized_data.buff_items, &serialized_data.reinforce_items})
        for (const CommonItemData &d : *list)
            if (has_quantity(d)) item::add_item(context, *target, d.id, d.amount);

    // Recovery Items
    for (const CommonItemData &d : serialized_data.recovery_items)
        if (has_quantity(d)) item::add_recovery_item(context, *target, d.id, d.amount);

    // Exchange point
    for (const CommonItemData &d : serialized_data.exchange)
        if (has_quantity(d)) exchange::add_exchange_point(context, *target, d.id, d.amount);

    // Achievement (note: it must be done last)
    std::map<int64_t, db::main::Achievement *> achievements_lookup;
    for (db::main::Achievement *ach : achievement::get_achievements(context, *target, std::nullopt))
        achievements_lookup[ach->achievement_id] = ach;
    for (const AchievementData &ach_sdata : serialized_data.achievement) {
        db::main::Achievement *ach_data;
        auto it = achievements_lookup.find(ach_sdata.achievement_id);
        if (it == achievements_lookup.end()) {
            const db::achievement::Achievement *ach_info =
                achievement::get_achievement_info(context, ach_sdata.achievement_id);
            ach_data = achievement::add_achievement(context, *target, *ach_info);
        } else {
            ach_data = it->second;
        }

        ach_data->count = ach_sdata.count;
        ach_data->is_accomplished = (ach_sdata.flags & 1) != 0;
        ach_data->is_reward_claimed = (ach_sdata.flags & 2) != 0;
        ach_data->is_new = (ach_sdata.flags & 4) != 0;
        ach_data->reset_value = ach_sdata.reset_value;
    }

    context.db.main.flush();
    return target;
}

}  // namespace npps::system::lila
