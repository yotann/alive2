#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include <memory>
#include <utility>

namespace llvm {
class CallInst;
class TargetLibraryInfo;
}

namespace IR {
class BasicBlock;
class Instr;
}

namespace llvm_util {

// returned bool indicates whether it's a known function call
std::pair<std::unique_ptr<IR::Instr>, bool>
known_call(llvm::CallInst &i, const llvm::TargetLibraryInfo &TLI,
           IR::BasicBlock &BB);

}