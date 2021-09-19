// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "ir/state.h"
#include "util/config.h"
#include "util/interp.h"
#include "util/concreteval.h"
#include "util/random.h"
#include <iostream>

using namespace IR;
using namespace std;
using util::config::dbg;

namespace util {

shared_ptr<ConcreteVal> Interpreter::getInputValue(unsigned index,
                                                   const Input &i) {
  if (i.getType().isIntType()) {
    // Comment this to avoid random int function arguments
    // auto rand_int64 = get_random_int64();
    // cout << "input param random value = " << rand_int64 << "\n";
    return shared_ptr<ConcreteVal>(
        new ConcreteValInt(false, llvm::APInt(i.getType().bits(), 3)));
  } else if (i.getType().isFloatType()) {
    cout << "float input encountered " << '\n';

    if (i.bits() == 32) {
      return shared_ptr<ConcreteVal>(
          new ConcreteValFloat(false, llvm::APFloat(3.0f)));
    } else if (i.bits() == 64) {
      return shared_ptr<ConcreteVal>(
          new ConcreteValFloat(false, llvm::APFloat(3.0)));
    } else if (i.bits() == 16) {
      return shared_ptr<ConcreteVal>(new ConcreteValFloat(
          false, llvm::APFloat(llvm::APFloatBase::IEEEhalf(), "3.0")));
    } else {
      cout << "AliveExec-Error : Unsupported float input type. Aborting"
           << '\n';
      return nullptr;
    }
  } else if (i.getType().isVectorType()) {
    return shared_ptr<ConcreteVal>(new ConcreteValAggregate(false, &i));
  } else {
    cout << "AliveExec-Error : input type not supported" << '\n';
    return nullptr;
  }
}

Interpreter::Interpreter() {}

void Interpreter::start(Function &f) {
  // TODO need to check for Value subclasses for inputs and constants
  // i.e. PoisonValue, UndefValue, and etc.
  // initialize inputs with concrete values

  concrete_vals[&Value::voidVal] = make_shared<ConcreteValVoid>();

  unsigned input_i = 0;
  for (auto &i : f.getInputs()) {
    auto I = concrete_vals.find(&i);
    assert(I == concrete_vals.end());
    auto new_val = getInputValue(input_i++, *dynamic_cast<const Input *>(&i));
    if (!new_val) {
      unsupported_flag = true;
      return;
    }
    concrete_vals.emplace(&i, new_val);
  }
  for (auto &i : f.getConstants()) {
    auto I = concrete_vals.find(&i);
    assert(I == concrete_vals.end());
    // read poison consts
    if (dynamic_cast<const PoisonValue *>(&i)) {
      // TODO add a concreteValPoison sublcass maybe 
      auto new_val = new ConcreteValInt(true, llvm::APInt(i.getType().bits(), 0));
      concrete_vals.emplace(&i, new_val);
    } else if (auto const_ptr = dynamic_cast<const IntConst *>(&i)) {
      // auto const_ptr = dynamic_cast<const IntConst *>(&i);
      // cout << "constant: " << i.getName() << " type: " <<
      // i.getType().toString()
      //<< " bitwidth: " << i.getType().bits() << '\n';
      // need to do this for constants that are larger than i64
      if (const_ptr->getString()) {
        // cout << "encountered const stored as string: " <<
        // *(const_ptr->getString())<< '\n';
        auto new_val = new ConcreteValInt(
            false,
            llvm::APInt(i.getType().bits(), *(const_ptr->getString()), 10));
        concrete_vals.emplace(&i, new_val);
      } else if (const_ptr->getInt()) {
        // cout << "encountered const stored as int: " <<
        // *(const_ptr->getInt())<< '\n';
        auto new_val = new ConcreteValInt(
            false, llvm::APInt(i.getType().bits(), *(const_ptr->getInt())));
        concrete_vals.emplace(&i, new_val);
      }
    } else if (auto const_ptr = dynamic_cast<const FloatConst *>(&i)) {
      assert((const_ptr->bits() == 32) || (const_ptr->bits() == 64) ||
             (const_ptr->bits() == 16)); // TODO: add support for other FP types
      //cout << "float constant: " << const_ptr->getName()
      //     << " type: " << i.getType().toString()
      //     << " bitwidth: " << i.getType().bits() << '\n';
      if (auto double_float = const_ptr->getDouble()) {
        //cout << "double repr of float constant : " << *double_float << '\n';
        if (const_ptr->bits() == 32) {
          // Since the original ir was representable using float, casting the
          // double back to float should be safe I think but this looks pretty
          // bad and should think of a better solution
          auto new_val = new ConcreteValFloat(
              false, llvm::APFloat(static_cast<float>(*double_float)));
          concrete_vals.emplace(&i, new_val);
        } else if (const_ptr->bits() == 64) {
          auto new_val = new ConcreteValFloat(false, llvm::APFloat(*double_float));
          concrete_vals.emplace(&i, new_val);
        } else {
          UNREACHABLE();
        }
      }
      if (auto string_float = const_ptr->getString()) {
        cout << "string repr of float constant : " << *string_float << '\n';
      } else if (auto int_float = const_ptr->getInt()) {

        auto new_val = new ConcreteValFloat(false,
                                  llvm::APFloat(llvm::APFloatBase::IEEEhalf(),
                                                llvm::APInt(16, *int_float)));
        concrete_vals.emplace(&i, new_val);

        cout << "int repr of float constant : " << *int_float << '\n';
      }

    } else { // TODO for now we only support Int constants
      cout << "AliveExec-Error : Unsupported constant type. Aborting!" << '\n';
      unsupported_flag = true;
      return;
    }
  }

  // TODO add stack for alloca

  // TODO add support for noninteger types

  for (auto &bb : f.getBBs()) {
    if (&f.getFirstBB() == bb) {
      cur_block = bb;
      break;
    }
  }
}

void Interpreter::step() {
  // Interpreter returns with a message as soon as it encounters undef
  // Hence we only need to deal with defined and poison vals during
  // interpretation
  if (isFinished())
    return;
  auto &i = *(cur_block->instrs().begin() + pos_in_block++);
  cout << "cur inst: ";
  i.print(cout);
  cout << '\n';
  if (dynamic_cast<const Return *>(&i)) {
    auto v_op = i.operands();
    assert(v_op.size() == 1);
    assert(concrete_vals.find(v_op[0]) != concrete_vals.end());
    return_value = concrete_vals[v_op[0]].get();
    cout << "Interpreter return result:" << '\n';
    return_value->print();
    cur_block = nullptr;
    return;
  } else if (dynamic_cast<const JumpInstr *>(&i)) {
    // cout << "jump inst: " << i << '\n';
    if (auto br_ptr = dynamic_cast<const Branch *>(&i)) {
      if (!br_ptr->getCondPtr()) { // unconditional branch
        // cout << "unconditional branch" << '\n';
        assert(br_ptr->getTruePtr());
        pred_block = cur_block;
        cur_block = br_ptr->getTruePtr();
        pos_in_block = 0;
        return;
      } else {
        assert(br_ptr->getTruePtr() && br_ptr->getFalsePtr());
        // cout << "conditional branch" << '\n';
        // lookup the concrete value of cond from concrete_vals
        // if true set cur_block to dst_true BB else dst_false

        auto I = concrete_vals.find(br_ptr->getCondPtr());
        assert(
            I !=
            concrete_vals.end()); // condition must be evaluated at this point
        auto concrete_cond_val = I->second.get();
        if (concrete_cond_val->isPoison()) {
          UB_flag = true;
          cout << "branch condition val is poison. This results in UB" << '\n';
          return;
        } else {
          pred_block = cur_block;
          auto condVar = dynamic_cast<ConcreteValInt *>(concrete_cond_val);
          assert(condVar);
          cur_block = condVar->getBoolVal() ? br_ptr->getTruePtr()
                                            : br_ptr->getFalsePtr();
          pos_in_block = 0;
        }
      }
    }
  } else if (auto phi_ptr = dynamic_cast<const Phi *>(&i)) {
    assert(pred_block);
    const auto &phi_vals = phi_ptr->getValues();
    // read pred_block to determine which phi value to choose
    for (auto &[phi_val_ptr, phi_label] : phi_vals) {
      // What to do if the phi_val is poison?
      if (pred_block->getName() != phi_label)
        continue;
      auto phi_val_concrete_I = concrete_vals.find(phi_val_ptr);
      assert(phi_val_concrete_I != concrete_vals.end());
      concrete_vals[phi_ptr] = phi_val_concrete_I->second;
      break;
    }
  } else {
    auto res_val = i.concreteEval(*this);
    if (!res_val) {
      cout << "AliveExec-Error : unsupported instruction. Aborting" << '\n';
      unsupported_flag = true;
      return;
    }
    concrete_vals[&i] = res_val;
  }
}

void Interpreter::run(unsigned instr_limit) {
  while (instr_limit--) {
    if (isFinished())
      return;
    step();
  }
}

Interpreter::~Interpreter() {}

void interp(Function &f) {
  cout << "running interp.cpp" << '\n';
  Interpreter interpreter;
  interpreter.start(f);

  cout << "---run Interpreter---" << '\n';
  interpreter.run();
  if (!interpreter.isFinished()) {
    cout << "ERROR: Interpreter reached instruction limit" << '\n';
    exit(EXIT_FAILURE);
  }
  if (interpreter.isUnsupported()) {
    exit(EXIT_FAILURE);
  }

  cout << "---Interpreter done---" << '\n';
}

} // namespace util
