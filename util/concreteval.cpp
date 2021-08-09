// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/concreteval.h"

using namespace std;
using namespace IR;

namespace util{

  ConcreteVal::ConcreteVal(bool poison) {
    setPoison(poison);
  }

  //ConcreteVal::ConcreteVal(bool poison, llvm::APInt val) 
  //: val(val) {
  //  setPoison(poison);
  //}

  //ConcreteVal::ConcreteVal(bool poison, llvm::APFloat val) 
  //: val(val) {
  //  setPoison(poison);
  //}

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

  void ConcreteVal::setVal(ConcreteVal& v){
     *this = v;
  }

  //void ConcreteVal::setVal(llvm::APInt& v){
  //  val = v;
  //}

  //void ConcreteVal::setVal(llvm::APFloat& v){
  //  val = v;
  //}

  ConcreteVal& ConcreteVal::getVal() {
    return *this;

  //  return *(std::get_if<llvm::APInt>(&val));
  }

  //llvm::APFloat& ConcreteVal::getValFloat(){
  //  return *(std::get_if<llvm::APFloat>(&val));
  //}

  void ConcreteVal::print(){
    cout << "!!! Should not happend " << '\n';
    //if (auto val_ptr = std::get_if<llvm::APInt>(&val)){
    //  llvm::SmallString<40> S, U;
    //  val_ptr->toStringUnsigned(U);
    //  val_ptr->toStringSigned(S);
    //  std::cout << "ConcreteVal( poison=" << isPoison() << ", " << val_ptr->getBitWidth() << "b, "
    //            << U.c_str() << "u " << S.c_str() << "s)\n";
    //}
    //else if (auto val_ptr = std::get_if<llvm::APFloat>(&val)){
    //  llvm::SmallVector<char, 16> Buffer;
    //  val_ptr->toString(Buffer);
    //  auto bits = val_ptr->getSizeInBits(val_ptr->getSemantics());
    //  std::string F(Buffer.begin(),Buffer.end());
    //  std::cout << "ConcreteVal( poison=" << isPoison() << ", " 
    //            << bits << "b, " << F << "F)\n";
    //}
    //else{
    //  UNREACHABLE();
    //}
    
    return;
  }

  ConcreteValInt::ConcreteValInt(bool poison, llvm::APInt &&val)
  : ConcreteVal(poison), val(move(val)) {
  }

  ConcreteVal& ConcreteValInt::getVal(){
    return *this;
  }

  bool ConcreteValInt::getBoolVal() {
    return val.getBoolValue();
  }

  void ConcreteValInt::print() {
    llvm::SmallString<40> S, U;
    val.toStringUnsigned(U);
    val.toStringSigned(S);
    std::cout << "ConcreteVal(poison=" << isPoison() << ", " << val.getBitWidth() << "b, "
              << U.c_str() << "u, " << S.c_str() << "s)\n";
  }

  ConcreteVal* ConcreteValInt::evalPoison(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    if (lhs->isPoison() || rhs->isPoison()) {
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValInt::add(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    bool nsw_flag = flags & BinOp::Flags::NSW;
    bool nuw_flag = flags & BinOp::Flags::NUW;
  
    auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val));
    bool ov_flag_s = false;
    bool ov_flag_u = false;
    auto ap_sum_s = v->val.sadd_ov(rhs_int->val, ov_flag_s);
    auto ap_sum_u = v->val.uadd_ov(rhs_int->val, ov_flag_u);
    v->val = ap_sum_s;
    if ((nsw_flag & ov_flag_s) || (nuw_flag & ov_flag_u)){
      v->setPoison(true);
    }
    return v;
  }

  ConcreteVal* ConcreteValInt::sAddSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto sadd_res = lhs_int->val.sadd_sat(rhs_int->val);
    auto v = new ConcreteValInt(false, move(sadd_res));
    return v;
  }
  
  ConcreteVal* ConcreteValInt::uAddSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto uadd_res = lhs_int->val.uadd_sat(rhs_int->val);
    auto v = new ConcreteValInt(false, move(uadd_res));
    return v;
  }

  ConcreteVal* ConcreteValInt::sSubSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto ssub_res = lhs_int->val.ssub_sat(rhs_int->val);
    auto v = new ConcreteValInt(false, move(ssub_res));
    return v;
  }

  ConcreteVal* ConcreteValInt::uSubSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto usub_res = lhs_int->val.usub_sat(rhs_int->val);
    auto v = new ConcreteValInt(false, move(usub_res));
    return v;
  } 
  
  ConcreteVal* ConcreteValInt::sShlSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    uint64_t max_shift = (1<<23);
    auto lhs_bitwidth = lhs_int->val.getBitWidth();
    auto shift_amt_u_limit = rhs_int->val.getLimitedValue(max_shift);
    if ((shift_amt_u_limit == max_shift) ||
        (shift_amt_u_limit >= lhs_bitwidth)){
      auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val));
      v->setPoison(true);
      return v;
    }
    
    auto sshl_res = lhs_int->val.sshl_sat(rhs_int->val);
    auto v = new ConcreteValInt(false, move(sshl_res));
    return v;
  }

  ConcreteVal* ConcreteValInt::uShlSat(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    uint64_t max_shift = (1<<23);
    auto lhs_bitwidth = lhs_int->val.getBitWidth();
    auto shift_amt_u_limit = rhs_int->val.getLimitedValue(max_shift);
    if ((shift_amt_u_limit == max_shift) ||
        (shift_amt_u_limit >= lhs_bitwidth)){
      auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val));
      v->setPoison(true);
      return v;
    }
    
    auto sshl_res = lhs_int->val.ushl_sat(rhs_int->val);
    auto v = new ConcreteValInt(false, move(sshl_res));
    return v;
  }
  //uShlSat

  ConcreteValFloat::ConcreteValFloat(bool poison, llvm::APFloat &&val)
  : ConcreteVal(poison), val(move(val)) {

  }

  ConcreteValFloat& ConcreteValFloat::getVal(){
    return *this;
  }

  ConcreteVal* ConcreteValFloat::evalPoison(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
  
    if (lhs->isPoison() || rhs->isPoison()) {
      auto v = new ConcreteValFloat(true, llvm::APFloat(lhs_float->val));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValFloat::binOPEvalFmath(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);

    if (fmath.isNNan()){
      if (lhs_float->val.isNaN() || rhs_float->val.isNaN()){
        auto v = new ConcreteValFloat(true, llvm::APFloat(lhs_float->val));
        return v;
      }
    }

    if (fmath.isNInf()){ 
      if (lhs_float->val.isInfinity() || rhs_float->val.isInfinity()){
        auto v = new ConcreteValFloat(true, llvm::APFloat(lhs_float->val));
        return v;
      }
    }

    return nullptr;
  }

  void ConcreteValFloat::unOPEvalFmath(ConcreteVal* n, IR::FastMathFlags fmath) {
    auto n_float = dynamic_cast<ConcreteValFloat *>(n);

    if (fmath.isNNan() && n_float->val.isNaN()){
      n_float->setPoison(true);
    }
    
    if (fmath.isNInf() && n_float->val.isInfinity()){
      n_float->setPoison(true);
    }
  }

  ConcreteVal* ConcreteValFloat::fadd(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = binOPEvalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    auto status = v->val.add(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    assert(status == llvm::APFloatBase::opOK);
    unOPEvalFmath(v, fmath);
    return v;
    
  }

  ConcreteVal* ConcreteValFloat::fsub(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = binOPEvalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    auto status = v->val.subtract(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    assert(status == llvm::APFloatBase::opOK);
    unOPEvalFmath(v, fmath);
    return v;
    
  }

  ConcreteVal* ConcreteValFloat::fmul(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = binOPEvalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    auto status = v->val.multiply(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    assert(status == llvm::APFloatBase::opOK);
    unOPEvalFmath(v, fmath);
    return v;
    
  }

  ConcreteVal* ConcreteValFloat::fdiv(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = binOPEvalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    auto status = v->val.divide(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    assert(status == llvm::APFloatBase::opOK);
    unOPEvalFmath(v, fmath);
    return v;
    
  }

  ConcreteVal* ConcreteValFloat::frem(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = binOPEvalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    auto status = v->val.mod(rhs_float->val);
    assert(status == llvm::APFloatBase::opOK);
    unOPEvalFmath(v, fmath);
    return v;
    
  }

  ConcreteVal* ConcreteValFloat::fmax(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    if (lhs_float->val.isNaN() && rhs_float->val.isNaN()) {
      auto qnan = llvm::APFloat::getQNaN(lhs_float->val.getSemantics());
      auto v = new ConcreteValFloat(false, move(qnan));
      return v;
    }
    else if (lhs_float->val.isNaN()) {
      auto v = new ConcreteValFloat(false, llvm::APFloat(rhs_float->val));
      return v;
    }
    else if (rhs_float->val.isNaN()) {
      auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
      return v;
    }
    else{ // neither operand is nan
      auto compare_res = lhs_float->val.compare(rhs_float->val);
      if (compare_res == llvm::APFloatBase::cmpGreaterThan) {
        auto v = new ConcreteValFloat(*lhs_float);
        return v;
      }
      else {
        auto v = new ConcreteValFloat(*rhs_float);
        return v;
      }
    }
    UNREACHABLE();
  }

  ConcreteVal* ConcreteValFloat::fmin(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    if (lhs_float->val.isNaN() && rhs_float->val.isNaN()) {
      auto qnan = llvm::APFloat::getQNaN(lhs_float->val.getSemantics());
      auto v = new ConcreteValFloat(false, move(qnan));
      return v;
    }
    else if (lhs_float->val.isNaN()) {
      auto v = new ConcreteValFloat(false, llvm::APFloat(rhs_float->val));
      return v;
    }
    else if (rhs_float->val.isNaN()) {
      auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
      return v;
    }
    else{ // neither operand is nan
      auto compare_res = lhs_float->val.compare(rhs_float->val);
      if (compare_res == llvm::APFloatBase::cmpGreaterThan) {
        auto v = new ConcreteValFloat(*rhs_float);
        return v;
      }
      else {
        auto v = new ConcreteValFloat(*lhs_float);
        return v;
      }
    }
    UNREACHABLE();
  }

  ConcreteVal* ConcreteValFloat::fmaximum(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    ConcreteVal* v = nullptr;
    if (lhs_float->val.isNaN() || rhs_float->val.isNaN()) {
      auto qnan = llvm::APFloat::getQNaN(lhs_float->val.getSemantics());
      v = new ConcreteValFloat(false, move(qnan));
    }
    else if (lhs_float->val.isZero() && rhs_float->val.isZero()) {
      if (lhs_float->val.isNegZero() && rhs_float->val.isNegZero()) 
        v = new ConcreteValFloat(*lhs_float);
      else 
        v = new ConcreteValFloat(false, llvm::APFloat::getZero(lhs_float->val.getSemantics(), false));
    }
    else{ 
      auto compare_res = lhs_float->val.compare(rhs_float->val);
      if (compare_res == llvm::APFloatBase::cmpGreaterThan) {
        auto v = new ConcreteValFloat(*lhs_float);
        return v;
      }
      else {
        auto v = new ConcreteValFloat(*rhs_float);
        return v;
      }
    }
    return v;
  }

  ConcreteVal* ConcreteValFloat::fminimum(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    ConcreteVal* v = nullptr;
    if (lhs_float->val.isNaN() || rhs_float->val.isNaN()) {
      auto qnan = llvm::APFloat::getQNaN(lhs_float->val.getSemantics());
      v = new ConcreteValFloat(false, move(qnan));
    }
    else if (lhs_float->val.isZero() && rhs_float->val.isZero()) {
      if (lhs_float->val.isPosZero() && rhs_float->val.isPosZero()) 
        v = new ConcreteValFloat(*lhs_float);
      else 
        v = new ConcreteValFloat(false, llvm::APFloat::getZero(lhs_float->val.getSemantics(), true));
    }
    else{ 
      auto compare_res = lhs_float->val.compare(rhs_float->val);
      if (compare_res == llvm::APFloatBase::cmpGreaterThan) {
        auto v = new ConcreteValFloat(*rhs_float);
        return v;
      }
      else {
        auto v = new ConcreteValFloat(*lhs_float);
        return v;
      }
    }
    return v;
  }

  void ConcreteValFloat::print() { 
    llvm::SmallVector<char, 16> Buffer;
    val.toString(Buffer);
    auto bits = val.getSizeInBits(val.getSemantics());
    std::string F(Buffer.begin(),Buffer.end());
    std::cout << "ConcreteVal(poison=" << isPoison() << ", " 
              << bits << "b, " << F << "F)\n";
  }

  ConcreteValVect::ConcreteValVect(bool poison, std::vector<ConcreteVal*> &&elements)
  : ConcreteVal(poison), elements(move(elements)) {
    assert(this->elements.size() > 0);
  }

  ConcreteValVect::ConcreteValVect(bool poison, const IR::Value* vect_val)
  : ConcreteVal(poison) {
    assert(vect_val->getType().isVectorType());
    auto vect_type_ptr = dynamic_cast<const VectorType *>(&vect_val->getType());
    // I don't think vector type can have padding
    assert(vect_type_ptr->numPaddingsConst() == 0);
    auto bitwidth =  vect_type_ptr->getChild(0).bits();
    auto isIntTy = vect_type_ptr->getChild(0).isIntType();
    
    for (unsigned int i=0; i < vect_type_ptr->numElementsConst(); ++i) {
      if (isIntTy){
        auto *v = new ConcreteValInt(false, llvm::APInt(bitwidth, 3));
        elements.push_back(v);
      }
      else{
        assert( "Error: vector type not supported yet!" && false );
      }  
    }

  }

  //ConcreteValVect::ConcreteValVect(ConcreteValVect &l){
  //  cout << "copy ctor concreteValVect" << '\n';
  //}
//
  //ConcreteValVect& ConcreteValVect::operator=(ConcreteValVect &l) {
  //  cout << "assign op concreteValVect" << '\n';
  //  return *this;
  //}
//
  //ConcreteValVect::ConcreteValVect(ConcreteValVect &&l){
  //  cout << "move ctor concreteValVect" << '\n';
  //}
//
  //ConcreteValVect& ConcreteValVect::operator=(ConcreteValVect &&l) {
  //  cout << "move assign op concreteValVect" << '\n';
  //  return *this;
  //}

  ConcreteValVect& ConcreteValVect::getVal() {
    return *this;
  }

  ConcreteValVect::~ConcreteValVect(){
    for (auto elem : elements) {
      delete elem;
    }
    elements.clear();
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
        res[i] = new ConcreteValInt(false, llvm::APInt(bitwidth, 3));
      }
      else{
        assert( "Error: vector type not supported yet!" && false );
      }  
    }
  
    return res;
  }

  unique_ptr<vector<ConcreteVal*>> ConcreteValVect::make_elements_unique(Value* vect_val){
    auto res = make_unique<vector<ConcreteVal*>>();
    return res;
  }

  void ConcreteValVect::print() {
    cout << "<" << '\n';
    for (auto elem : elements) {
      elem->print();
    }
    cout << ">" << '\n';
  }

}