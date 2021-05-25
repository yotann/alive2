#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallString.h"
#include <memory>
#include <variant>
#include <iostream>

namespace util {
  class ConcreteVal{
  private:
    bool poison;
    llvm::APInt val;
  public:
    ConcreteVal(): poison(false), val() {}
    ConcreteVal(bool poison, llvm::APInt val): poison(poison), val(val) {}
    ConcreteVal(ConcreteVal& l) = default;
    ConcreteVal& operator=(ConcreteVal& l) = default;
    ConcreteVal( ConcreteVal&& r) = default;
    ConcreteVal& operator=(ConcreteVal&& r) = default;
    void setPoison(bool poison);
    bool isPoison();
    void setVal(llvm::APInt& v);
    llvm::APInt& getVal();
    void setConcreteVal(bool poison, std::unique_ptr<llvm::APInt> new_val_ptr);
    void print();
  };

}