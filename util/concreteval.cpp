// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/concreteval.h"

namespace util{
  void ConcreteVal::setPoison(bool poison){
    this->poison = poison;
  }

  bool ConcreteVal::isPoison(){
    return this->poison;
  }

  /*void ConcreteVal::setValPtr(std::unique_ptr<llvm::APInt> new_val_ptr){
    val_ptr = std::move(new_val_ptr);
  }*/
  
  /*std::unique_ptr<llvm::APInt> ConcreteVal::getValPtr(){
    return val_ptr;
  }*/

  void ConcreteVal::setVal(llvm::APInt& v){
    val = v;
  }

  llvm::APInt& ConcreteVal::getVal(){
    return val;
  }

  /*void ConcreteVal::setConcreteVal(bool poison, std::unique_ptr<llvm::APInt> new_val_ptr){
    setPoison(poison);
    setValPtr(new_val_ptr);
  }*/

  void ConcreteVal::print(){
    llvm::SmallString<40> S, U;
    val.toStringUnsigned(U);
    val.toStringSigned(S);
    std::cout << "ConcreteVal( poison=" << poison << ", " << val.getBitWidth() << "b, "
         << U.c_str() << "u " << S.c_str() << "s)\n";
    return;
  }
}