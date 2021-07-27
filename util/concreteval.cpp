// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/concreteval.h"

namespace util{

  ConcreteVal::ConcreteVal(bool poison, llvm::APInt val) 
  : val(val) {
    setPoison(poison);
  }

  ConcreteVal::ConcreteVal(bool poison, llvm::APFloat val) 
  : val(val) {
    setPoison(poison);
  }

  void ConcreteVal::setPoison(bool poison){
    if (poison) {
      this->flags = Flags::Poison;
    }
    else {
      this->flags = Flags::None;
    }
    //this->poison = poison;
  }

  void ConcreteVal::setUndef(){
    this->flags = Flags::Undef;
  }

  bool ConcreteVal::isPoison(){
    return flags & Flags::Poison;
    //return this->poison;
  }

  bool ConcreteVal::isUndef(){
    return flags & Flags::Undef;
  }

  void ConcreteVal::setVal(llvm::APInt& v){
    val = v;
  }

  void ConcreteVal::setVal(llvm::APFloat& v){
    val = v;
  }

  llvm::APInt& ConcreteVal::getVal(){
    return *(std::get_if<llvm::APInt>(&val));
  }

  llvm::APFloat& ConcreteVal::getValFloat(){
    return *(std::get_if<llvm::APFloat>(&val));
  }

  void ConcreteVal::print(){
    if (auto val_ptr = std::get_if<llvm::APInt>(&val)){
      llvm::SmallString<40> S, U;
      val_ptr->toStringUnsigned(U);
      val_ptr->toStringSigned(S);
      std::cout << "ConcreteVal( poison=" << isPoison() << ", " << val_ptr->getBitWidth() << "b, "
                << U.c_str() << "u " << S.c_str() << "s)\n";
    }
    else if (auto val_ptr = std::get_if<llvm::APFloat>(&val)){
      llvm::SmallVector<char, 16> Buffer;
      val_ptr->toString(Buffer);
      auto bits = val_ptr->getSizeInBits(val_ptr->getSemantics());
      std::string F(Buffer.begin(),Buffer.end());
      std::cout << "ConcreteVal( poison=" << isPoison() << ", " 
                << bits << "b, " << F << "F)\n";
    }
    else{
      UNREACHABLE();
    }
    
    return;
  }
}