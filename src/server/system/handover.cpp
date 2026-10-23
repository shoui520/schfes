// npps4/system/handover.py
#include "system/handover.hpp"

#include "core/crypto.hpp"
#include "core/util.hpp"

#include <vector>

namespace npps::system::handover {

static std::string a_sha1(const std::string &t) { return util::hex(crypto::sha1(t), true); }

std::string generate_passcode_sha1(const std::string &transfer_id, const std::string &transfer_code) {
    return a_sha1(a_sha1(transfer_id) + transfer_code);
}

std::string generate_transfer_code() {
    static const std::vector<char> population(VALID_CHARACTERS, VALID_CHARACTERS + 36);
    std::string out;
    for (char c : util::rng().choices(population, 12)) out.push_back(c);
    return out;
}

bool has_passcode_issued(db::main::User &user) { return user.transfer_sha1.has_value(); }

db::main::User *find_user_by_passcode(Context &context, const std::string &sha1_code) {
    return context.db.main.first<db::main::User>("WHERE \"transfer_sha1\" = ?", sha1_code);
}

void swap_credentials(db::main::User &source_user, db::main::User &target_user) {
    // These handles if the source and target is same.
    std::optional<std::string> key = source_user.key, passwd = source_user.passwd;
    source_user.key.reset();
    source_user.passwd.reset();
    target_user.key = key;
    target_user.passwd = passwd;
}

}  // namespace npps::system::handover
