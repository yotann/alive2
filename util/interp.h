#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <map>
#include <memory>
#include <vector>

namespace IR {
class BasicBlock;
class Function;
class Input;
class Value;
}

namespace util {

class ConcreteVal;

class Interpreter {
public:
  Interpreter();
  virtual ~Interpreter();
  void start(IR::Function &f);
  void step();
  void run(unsigned instr_limit = 100);
  virtual std::shared_ptr<ConcreteVal> getInputValue(unsigned index,
                                                     const IR::Input &input);

  static ConcreteVal *getPoisonValue(const IR::Type &type);
  static const llvm::fltSemantics *getFloatSemantics(const IR::FloatType &type);

  bool isReturned() const {
    return !cur_block;
  }

  bool isUndefined() const {
    return UB_flag;
  }

  bool isUnsupported() const {
    return unsupported_flag;
  }

  bool isFinished() const {
    return isReturned() || isUndefined() || isUnsupported();
  }

  std::map<const IR::Value *, std::shared_ptr<ConcreteVal>> concrete_vals;
  const IR::BasicBlock *pred_block = nullptr;
  const IR::BasicBlock *cur_block = nullptr;
  unsigned pos_in_block = 0;
  bool UB_flag = false;
  bool unsupported_flag = false;
  ConcreteVal *return_value = nullptr;
};

void interp(IR::Function &f);

}
