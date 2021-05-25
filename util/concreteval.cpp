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

  void ConcreteVal::setVal(llvm::APInt& v){
    val = v;
  }

  llvm::APInt& ConcreteVal::getVal(){
    return val;
  }

  void ConcreteVal::print(){
    llvm::SmallString<40> S, U;
    val.toStringUnsigned(U);
    val.toStringSigned(S);
    std::cout << "ConcreteVal( poison=" << poison << ", " << val.getBitWidth() << "b, "
         << U.c_str() << "u " << S.c_str() << "s)\n";
    return;
  }
}