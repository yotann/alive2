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
    enum Flags {
      None = 0 , Poison = 1 << 0, Undef = 1 << 1
    };
    unsigned flags = Flags::None;
    //bool poison;
    std::variant<llvm::APInt, llvm::APFloat> val;
  public:
    ConcreteVal(): flags(Flags::None), val() {}
    ConcreteVal(bool poison, llvm::APInt val);
    ConcreteVal(bool poison, llvm::APFloat val);
    ConcreteVal(ConcreteVal& l) = default;
    ConcreteVal& operator=(ConcreteVal& l) = default;
    ConcreteVal( ConcreteVal&& r) = default;
    ConcreteVal& operator=(ConcreteVal&& r) = default;
    void setPoison(bool poison);
    void setUndef();
    bool isPoison();
    bool isUndef();
    void setVal(llvm::APInt& v);
    void setVal(llvm::APFloat& v);
    llvm::APInt& getVal();
    llvm::APFloat& getValFloat();
    void print();
  };

}