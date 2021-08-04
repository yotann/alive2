// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/concreteval.h"

using namespace std;
using namespace IR;

namespace util{

  ConcreteVal::ConcreteVal(bool poison) {
    setPoison(poison);
  }

  ConcreteVal::ConcreteVal(bool poison, llvm::APInt val) 
  : val(val) {
    setPoison(poison);
  }

  ConcreteVal::ConcreteVal(bool poison, llvm::APFloat val) 
  : val(val) {
    setPoison(poison);
  }

  ConcreteVal::~ConcreteVal() {}

  void ConcreteVal::setPoison(bool poison){
    if (poison) {
      this->flags = Flags::Poison;
    }
    else {
      this->flags = Flags::None;
    }
  }

  void ConcreteVal::setUndef(){
    this->flags = Flags::Undef;
  }

  bool ConcreteVal::isPoison(){
    return flags & Flags::Poison;
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

  ConcreteValVect::ConcreteValVect(bool poison, std::vector<ConcreteVal*> &&elements)
  : ConcreteVal(poison), elements(move(elements)) {
    assert(elements.size() > 0);
  }

  //ConcreteValVect::ConcreteValVect(bool poison, std::vector<ConcreteVal*> &elements)
  //: ConcreteVal(poison), elements(elements) {
  //  assert(elements.size() > 0);
  //}

  vector<ConcreteVal*> ConcreteValVect::make_elements(const Value* vect_val) {
    assert(vect_val->getType().isVectorType());
    
    auto vect_type_ptr = dynamic_cast<const VectorType *>(&vect_val->getType());
    // I don't think vector type can have padding
    assert(vect_type_ptr->numPaddingsConst() == 0);
    //vect
    cout << "vect_num_elem: " << vect_type_ptr->numElementsConst() << '\n';
    //cout << ptr->numElementsConst() << "," << ptr->bits() 
    //  << "," << ptr->getChild(0).bits() << '\n';
    vector<ConcreteVal*> res(vect_type_ptr->numElementsConst());
    auto bitwidth =  vect_type_ptr->getChild(0).bits();
    auto isIntTy = vect_type_ptr->getChild(0).isIntType();
    for (unsigned int i=0; i < res.size(); ++i) {
      if (isIntTy){
        res[i] = new ConcreteVal(false, llvm::APInt(bitwidth, 3));
      }
      else{
        assert( "Error: vector type not supported yet!" && false );
      }  
    }
    
    cout << res.size() << '\n';
    cout << bitwidth << '\n';
    cout << isIntTy << '\n';
    return res;
  }

  unique_ptr<vector<ConcreteVal*>> ConcreteValVect::make_elements_unique(Value* vect_val){
    auto res = make_unique<vector<ConcreteVal*>>();
    return res;
  }



}