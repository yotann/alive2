#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "util/compiler.h"
#include <memory>
#include <variant>
#include <iostream>

namespace util {
  class ConcreteVal{
  private:
    bool poison;
    std::variant<llvm::APInt, llvm::APFloat> val;
  public:
    ConcreteVal(): poison(false), val() {}
    ConcreteVal(bool poison, llvm::APInt val): poison(poison), val(val) {}
    ConcreteVal(bool poison, llvm::APFloat val): poison(poison), val(val) {}
    ConcreteVal(ConcreteVal& l) = default;
    ConcreteVal& operator=(ConcreteVal& l) = default;
    ConcreteVal( ConcreteVal&& r) = default;
    ConcreteVal& operator=(ConcreteVal&& r) = default;
    void setPoison(bool poison);
    bool isPoison();
    void setVal(llvm::APInt& v);
    void setVal(llvm::APFloat& v);
    llvm::APInt& getVal();
    llvm::APFloat& getValFloat();
    void print();
  };

}