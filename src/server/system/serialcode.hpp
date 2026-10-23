#pragma once
// npps4/serialcode/__init__.py and npps4/serialcode/func.py
#include "app/context.hpp"
#include "app/models.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>

namespace npps::serialcode {

using Function = std::function<std::optional<std::string>(Context &, db::main::User &)>;
extern const std::map<std::string, Function> functions;

// Returns the result message; throws std::invalid_argument (ValueError) on usage limit / bad action.
std::string execute(Context &context, db::main::User &user, const std::string &input_code,
                    const data::schema::SerialCode &serial_code);

namespace func {
std::optional<std::string> give_all_supporter_units(Context &context, db::main::User &user);
}  // namespace func

}  // namespace npps::serialcode
