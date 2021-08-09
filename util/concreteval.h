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
#include "ir/instr.h"
#include <memory>
#include <variant>
#include <iostream>
#include <vector>
#include <utility>

namespace IR {
  struct FastMathFlags;
}

namespace util {
  class ConcreteVal{
  protected:
    enum Flags {
      None = 0 , Poison = 1 << 0, Undef = 1 << 1
    };
    unsigned flags = Flags::None;
    //bool poison;
    //std::variant<llvm::APInt, llvm::APFloat> val;
  public:
    //ConcreteVal(): flags(Flags::None), val() {}
    ConcreteVal(bool poison);
    //ConcreteVal(bool poison, llvm::APInt val);
    //ConcreteVal(bool poison, llvm::APFloat val);
    ConcreteVal(ConcreteVal& l) = default;
    ConcreteVal& operator=(ConcreteVal& l) = default;
    ConcreteVal( ConcreteVal&& r) = default;
    ConcreteVal& operator=(ConcreteVal&& r) = default;
    virtual ~ConcreteVal();
    virtual void setPoison(bool poison);
    virtual void setUndef();
    virtual bool isPoison();
    virtual bool isUndef();
    void setVal(ConcreteVal& v);
    //void setVal(llvm::APInt& v);
    //void setVal(llvm::APFloat& v);
    virtual ConcreteVal& getVal();
    //virtual llvm::APFloat& getValFloat() = 0;
    virtual void print();
  };

  class ConcreteValInt : public ConcreteVal {
    private:
    llvm::APInt val;
    public:
    //ConcreteValInt(bool poison, llvm::APInt val);
    ConcreteValInt(bool poison, llvm::APInt &&val);
    ConcreteVal& getVal() override;
    bool getBoolVal();
    void print() override;

    static ConcreteVal* evalPoison(ConcreteVal* lhs, ConcreteVal* rhs);
    static ConcreteVal* add(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
    static ConcreteVal* sAddSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
    static ConcreteVal* uAddSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
    static ConcreteVal* sSubSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
    static ConcreteVal* uSubSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
    static ConcreteVal* sShlSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
    static ConcreteVal* uShlSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags);
  }; 

  class ConcreteValFloat : public ConcreteVal {
    private:
    llvm::APFloat val;
    static ConcreteVal* evalPoison(ConcreteVal* lhs, ConcreteVal* rhs);
    static ConcreteVal* binOPEvalFmath(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static void unOPEvalFmath(ConcreteVal* n, IR::FastMathFlags fmath);
    public:
    //ConcreteValInt(bool poison, llvm::APInt val);
    ConcreteValFloat(bool poison, llvm::APFloat &&val);
    ConcreteValFloat& getVal() override;
    void print() override;

    static ConcreteVal* fadd(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fsub(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fmul(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fdiv(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* frem(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fmax(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fmin(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fmaximum(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
    static ConcreteVal* fminimum(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath);
  };

  class ConcreteValVect : public ConcreteVal {
  private:
  std::vector<ConcreteVal*> elements;
  public:
    
    ConcreteValVect(bool poison, std::vector<ConcreteVal*> &&elements);
    ConcreteValVect(bool poison, const IR::Value* vect_val);
    //ConcreteValVect(ConcreteValVect &l);
    //ConcreteValVect& operator=(ConcreteValVect &l);
    //ConcreteValVect( ConcreteValVect &&r);
    //ConcreteValVect& operator=(ConcreteValVect &&r);
    //ConcreteValVect(bool poison, std::vector<ConcreteVal*> &elements);
    ConcreteValVect& getVal() override;
    virtual ~ConcreteValVect();
    static std::vector<ConcreteVal*> make_elements(const IR::Value* vect_val);
    static std::unique_ptr<std::vector<ConcreteVal*>> make_elements_unique(IR::Value* vect_val);
    void print() override;
  };
  
}