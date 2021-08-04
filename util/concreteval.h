#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "util/compiler.h"
#include "ir/value.h"
#include "ir/type.h"
#include <memory>
#include <variant>
#include <iostream>
#include <vector>
#include <utility>

namespace util {
  class ConcreteVal{
  protected:
    enum Flags {
      None = 0 , Poison = 1 << 0, Undef = 1 << 1
    };
    unsigned flags = Flags::None;
    //bool poison;
    std::variant<llvm::APInt, llvm::APFloat> val;
  public:
    ConcreteVal(): flags(Flags::None), val() {}
    ConcreteVal(bool poison);
    ConcreteVal(bool poison, llvm::APInt val);
    ConcreteVal(bool poison, llvm::APFloat val);
    ConcreteVal(ConcreteVal& l) = default;
    ConcreteVal& operator=(ConcreteVal& l) = default;
    ConcreteVal( ConcreteVal&& r) = default;
    ConcreteVal& operator=(ConcreteVal&& r) = default;
    virtual ~ConcreteVal();
    virtual void setPoison(bool poison);
    virtual void setUndef();
    virtual bool isPoison();
    virtual bool isUndef();
    void setVal(llvm::APInt& v);
    void setVal(llvm::APFloat& v);
    llvm::APInt& getVal();
    llvm::APFloat& getValFloat();
    void print();
  };
  
  class ConcreteValVect : ConcreteVal {
  private:
    std::vector<ConcreteVal*> elements;
  public:
    ConcreteValVect(bool poison, std::vector<ConcreteVal*> &&elements);
    //ConcreteValVect(bool poison, std::vector<ConcreteVal*> &elements);
    static std::vector<ConcreteVal*> make_elements(const IR::Value* vect_val);
    static std::unique_ptr<std::vector<ConcreteVal*>> make_elements_unique(IR::Value* vect_val);

  };
  
}