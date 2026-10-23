#pragma once
// npps4/system/tutorial.py
#include "app/context.hpp"

namespace npps::system::tutorial {

void phase1(Context &context, db::main::User &u);
void phase2(Context &context, db::main::User &u);
void phase3(Context &context, db::main::User &u);
void finalize(Context &context, db::main::User &u);

}  // namespace npps::system::tutorial
