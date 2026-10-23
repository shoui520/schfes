// npps4/system/secretbox.py
#include "system/secretbox.hpp"

#include "system/item.hpp"
#include "system/user.hpp"

#include <map>
#include <stdexcept>

namespace npps::system::secretbox {

static std::string _determine_en_path(Context &context, const std::string &path, const std::optional<std::string> &path_en) {
    if (!path_en || context.is_lang_jp()) return path;

    if (path_en->empty()) return "en/" + path;
    return *path_en;
}

int64_t encode_cost_id(int64_t secretbox_id, int64_t button_index, int64_t cost_index) {
    return (button_index << 36) | (cost_index << 32) | secretbox_id;
}

std::tuple<int64_t, int64_t, int64_t> decode_cost_id(int64_t cost_id) {
    int64_t secretbox_id = cost_id & 0xFFFFFFFF;
    int64_t cost_index = (cost_id >> 32) & 0xF;
    int64_t button_index = cost_id >> 36;
    return {secretbox_id, button_index, cost_index};
}

model::Ptr<secretbox_model::SecretboxAllButton> query_secretbox_button(Context &context, db::main::User &user,
                                                                       const data::schema::SecretboxButton &button,
                                                                       const std::string &secretbox_name,
                                                                       std::tuple<int64_t, int64_t> ids) {
    std::vector<secretbox_model::SecretboxAllCost> costs;
    int64_t j = 1;
    for (const auto &cost : button.costs) {
        auto c = secretbox_model::SecretboxAllCost::make(
            encode_cost_id(std::get<0>(ids), std::get<1>(ids), j),
            get_user_currency(context, user, cost.cost_type, cost.cost_item_id) >= cost.cost_amount, button.unit_count,
            cost.cost_type, cost.cost_amount);
        c.item_id = cost.cost_item_id;
        costs.push_back(std::move(c));
        j++;
    }

    std::string name = secretbox_name;
    if (button.name) name = context.get_text(*button.name, button.name_en);

    if (button.balloon_asset) {
        auto b = secretbox_model::SecretboxAllButtonWithBaloon::make(button.button_type, std::move(costs), name);
        b.balloon_asset = _determine_en_path(context, *button.balloon_asset, button.balloon_asset_en);
        return model::make<secretbox_model::SecretboxAllButtonWithBaloon>(std::move(b));
    }
    return model::make<secretbox_model::SecretboxAllButton>(
        secretbox_model::SecretboxAllButton::make(button.button_type, std::move(costs), name));
}

std::vector<model::Ptr<secretbox_model::SecretboxAllButton>> get_secretbox_button_response(
    Context &context, db::main::User &target_user, const data::schema::SecretboxData &secretbox) {
    std::vector<model::Ptr<secretbox_model::SecretboxAllButton>> out;
    int64_t i = 1;
    for (const auto &button : secretbox.buttons) {
        // TODO: Free once a day scouting
        out.push_back(query_secretbox_button(context, target_user, button,
                                             context.get_text(secretbox.name, secretbox.name_en),
                                             {data::schema::secretbox_id(secretbox), i}));
        i++;
    }
    return out;
}

model::Ptr<secretbox_model::SecretboxAllSecretboxInfo> get_secretbox_info_response(
    Context &context, db::main::User &, const data::schema::SecretboxData &secretbox, bool can_do_more) {
    return model::make<secretbox_model::SecretboxAllSecretboxInfo>(secretbox_model::SecretboxAllSecretboxInfo::make(
        data::schema::secretbox_id(secretbox), secretbox.secretbox_type,
        context.get_text(secretbox.name, secretbox.name_en), util::timestamp_to_datetime(secretbox.start_time),
        util::timestamp_to_datetime(secretbox.end_time),
        0,  // TODO
        1, can_do_more ? 100 : 0));  // TODO
}

std::vector<secretbox_model::SecretboxAllMemberCategory> get_all_secretbox_data_response(Context &context,
                                                                                         db::main::User &target_user) {
    const data::ServerData &server_data = data::get();
    std::map<int64_t, std::vector<secretbox_model::SecretboxAllPage>> member_category_list;

    for (const auto &entry : server_data.secretbox_data) {
        const data::schema::SecretboxData &secretbox = entry.second;
        const auto &layout = secretbox.animation_asset_layout;
        const auto &layout_en = secretbox.animation_asset_layout_en;
        auto path = [&](size_t i) { return _determine_en_path(context, layout.at(i), layout_en.at(i)); };
        model::Ptr<secretbox_model::SecretboxAllAnimation2Asset> animation_assets;
        if (layout.size() > 3) {
            animation_assets = model::make<secretbox_model::SecretboxAllAnimation3Asset>(
                secretbox_model::SecretboxAllAnimation3Asset::make(secretbox.animation_layout_type, path(0), path(1),
                                                                   path(2), path(3)));
        } else {
            animation_assets = model::make<secretbox_model::SecretboxAllAnimation2Asset>(
                secretbox_model::SecretboxAllAnimation2Asset::make(secretbox.animation_layout_type, path(0), path(1),
                                                                   path(2)));
        }
        auto page = secretbox_model::SecretboxAllPage::make(
            _determine_en_path(context, secretbox.menu_asset, secretbox.menu_asset_en), secretbox.order,
            animation_assets, get_secretbox_button_response(context, target_user, secretbox),
            get_secretbox_info_response(context, target_user, secretbox, false));
        member_category_list[secretbox.member_category].push_back(std::move(page));
    }

    std::vector<secretbox_model::SecretboxAllMemberCategory> out;
    for (auto &kv : member_category_list)
        out.push_back(secretbox_model::SecretboxAllMemberCategory::make(kv.first, std::move(kv.second)));
    return out;
}

const data::schema::SecretboxData &get_secretbox_data(int64_t secretbox_id) {
    const data::ServerData &server_data = data::get();
    return server_data.secretbox_data.at(secretbox_id);
}

std::vector<int64_t> roll_units(int64_t secretbox_id, int64_t amount, int64_t guarantee_rarity,
                                int64_t guarantee_amount, const std::optional<std::vector<int64_t>> &rate_modifier) {
    const data::schema::SecretboxData &secretbox_data = get_secretbox_data(secretbox_id);
    // Calculate weighted probabilities
    const std::vector<int64_t> &rates = rate_modifier ? *rate_modifier : secretbox_data.rarity_rates;
    std::vector<int64_t> population;
    for (size_t i = 0; i < secretbox_data.rarity_rates.size(); i++) population.push_back((int64_t)i);
    std::vector<int64_t> picked_rarity_index = util::rng().choices(population, rates, (size_t)amount);

    if (guarantee_rarity > 0 && guarantee_amount > 0) {
        int64_t rindex = guarantee_rarity - 1;
        for (;;) {
            int64_t n = 0;
            for (int64_t k : picked_rarity_index) n += k >= rindex;
            if (n >= guarantee_amount) break;
            size_t random_index = util::rng().randbelow((uint64_t)amount);
            if (picked_rarity_index[random_index] < rindex) picked_rarity_index[random_index] = rindex;
        }
    }

    // Start rolling
    std::vector<int64_t> out;
    for (int64_t i : picked_rarity_index) out.push_back(util::rng().choice(secretbox_data.rarity_pools.at((size_t)i)));
    return out;
}

const data::schema::SecretboxButton &get_secretbox_button(const data::schema::SecretboxData &secretbox_data,
                                                          int64_t button_index) {
    int64_t idx = button_index - 1;
    if (idx < 0) idx += (int64_t)secretbox_data.buttons.size();  // Python negative indexing
    if (idx < 0 || idx >= (int64_t)secretbox_data.buttons.size()) throw std::out_of_range("list index out of range");
    return secretbox_data.buttons[(size_t)idx];
}

const data::schema::SecretboxButton &get_secretbox_button(int64_t secretbox_id, int64_t button_index) {
    return get_secretbox_button(get_secretbox_data(secretbox_id), button_index);
}

int64_t get_user_currency(Context &context, db::main::User &target_user, const_::SECRETBOX_COST_TYPE cost_type,
                          std::optional<int64_t> cost_item_id) {
    switch (cost_type) {
    case const_::SECRETBOX_COST_TYPE::ITEM_TICKET:
        if (!cost_item_id) throw std::invalid_argument("Empty item_id for type 1000");
        return item::get_item_count(context, target_user, *cost_item_id);
    case const_::SECRETBOX_COST_TYPE::GAME_COIN: return target_user.game_coin;
    case const_::SECRETBOX_COST_TYPE::LOVECA:
        return user::get_loveca(target_user, !cost_item_id || *cost_item_id != 1);
    case const_::SECRETBOX_COST_TYPE::FRIEND: return target_user.social_point;
    default: return 0;
    }
}

void sub_user_currency(Context &context, db::main::User &target_user, const_::SECRETBOX_COST_TYPE cost_type,
                       std::optional<int64_t> cost_item_id, int64_t amount) {
    switch (cost_type) {
    case const_::SECRETBOX_COST_TYPE::ITEM_TICKET:
        if (!cost_item_id) throw std::invalid_argument("Empty item_id for type 1000");
        item::add_item(context, target_user, *cost_item_id, -amount);
        break;
    case const_::SECRETBOX_COST_TYPE::GAME_COIN: target_user.game_coin = target_user.game_coin - amount; break;
    case const_::SECRETBOX_COST_TYPE::LOVECA:
        user::sub_loveca(target_user, amount, cost_item_id && *cost_item_id == 1);
        break;
    case const_::SECRETBOX_COST_TYPE::FRIEND: target_user.social_point = target_user.social_point - amount; break;
    default: break;
    }
}

}  // namespace npps::system::secretbox
