#pragma once
// external/badwords.py
#include "app/context.hpp"

#include <string>

namespace npps::external::badwords {

// True when `text` (lowercased, whitespace removed) contains a badword from server_data.json.
bool has_badwords(Context &context, const std::string &text);

}  // namespace npps::external::badwords
