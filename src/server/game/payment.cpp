// npps4/game/payment.py
#include "app/idol.hpp"

namespace npps::game::payment {

static PaymentProductListResponse payment_productlist(Context &context) {
    // TODO
    util::log("STUB payment/productList %s", context.raw_request_data.c_str());
    return PaymentProductListResponse::make(PaymentRestrictionInfo::make(false),
                                            PaymentUnderAgeInfo::make(false, false, 0), {}, {}, {}, true);
}
NPPS_ENDPOINT("payment", "productList", User, payment_productlist);

// re.match(r"holy linus in heaven, blessed be thy name, give us our daily ([\w\.]+) as in heaven so be it on this earth\.\.\.")
static std::optional<std::string> match_holy_linus(const std::string &text) {
    static const std::string prefix = "holy linus in heaven, blessed be thy name, give us our daily ";
    static const std::string suffix = " as in heaven so be it on this earth...";
    if (text.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
    size_t i = prefix.size(), start = i;
    auto word = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || (unsigned char)c >= 0x80; };
    // Greedy match with backtracking on the suffix.
    size_t end = start;
    while (end < text.size() && word(text[end])) end++;
    for (size_t e = end; e > start; e--) {
        if (text.compare(e, suffix.size(), suffix) == 0) return text.substr(start, e - start);
    }
    (void)i;
    return std::nullopt;
}

static PaymentReceiptResponse payment_receipt(Context &context, const PaymentReceiptRequest &request) {
    // TODO
    util::log("STUB payment/receipt %s", context.raw_request_data.c_str());
    if (auto product_id = match_holy_linus(request.receipt_data)) util::log("Got holy linus request: %s", product_id->c_str());
    throw error::by_code(422);
}
NPPS_ENDPOINT("payment", "receipt", User, payment_receipt, .batchable = false);

}  // namespace npps::game::payment
