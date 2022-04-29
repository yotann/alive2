// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/concreteval.h"
#include "util/interp.h"

using namespace std;
using namespace IR;

namespace util{

  ConcreteVal::ConcreteVal(bool poison) {
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

  bool ConcreteVal::isPoison() const {
    return flags & Flags::Poison;
  }

  bool ConcreteVal::isUndef() const {
    return flags & Flags::Undef;
  }

  ConcreteValVoid::ConcreteValVoid() : ConcreteVal(false) {}

  void ConcreteValVoid::print() {
    std::cout << "ConcreteValVoid()\n";
  }

  ConcreteValInt::ConcreteValInt(bool poison, llvm::APInt &&val)
  : ConcreteVal(poison), val(move(val)) {
  }

  llvm::APInt ConcreteValInt::getVal() const {
    return val;
  }

  void ConcreteValInt::setVal(llvm::APInt& v) {
    val = v;
  }

  bool ConcreteValInt::getBoolVal() const {
    return val.getBoolValue();
  }

  void ConcreteValInt::print() {
    llvm::SmallString<40> S, U;
    val.toStringUnsigned(U);
    val.toStringSigned(S);
    std::cout << "ConcreteVal(poison=" << isPoison() << ", " << val.getBitWidth() << "b, "
              << U.c_str() << "u, " << S.c_str() << "s)\n";
  }

  ConcreteVal* ConcreteValInt::evalPoison(ConcreteVal* op1, ConcreteVal* op2, ConcreteVal* op3) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op1);
    if (op1->isPoison() || op2->isPoison() || op3->isPoison()) {
      auto v = new ConcreteValInt(true, llvm::APInt(op_int->val.getBitWidth(),0));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValInt::evalPoison(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    if (lhs->isPoison() || rhs->isPoison()) {
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValInt::evalPoison(ConcreteVal* op) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);
    if (op->isPoison()) {
      auto v = new ConcreteValInt(true, llvm::APInt(op_int->val.getBitWidth(),0));
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

  ConcreteVal* ConcreteValInt::mul(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
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
    auto ap_mul_s = v->val.smul_ov(rhs_int->val, ov_flag_s);
    auto ap_mul_u = v->val.umul_ov(rhs_int->val, ov_flag_u);
    v->val = ap_mul_s;
    if ((nsw_flag & ov_flag_s) || (nuw_flag & ov_flag_u)){
      v->setPoison(true);
    }
    return v;
  }

  ConcreteVal* ConcreteValInt::sub(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
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
    auto ap_sub_s = v->val.ssub_ov(rhs_int->val, ov_flag_s);
    auto ap_sub_u = v->val.usub_ov(rhs_int->val, ov_flag_u);
    v->val = ap_sub_s;
    if ((nsw_flag & ov_flag_s) || (nuw_flag & ov_flag_u)){
      v->setPoison(true);
    }
    return v;   
  }

  ConcreteVal* ConcreteValInt::sdiv(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags, bool& UB_flag) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);

    if (!rhs_int->getBoolVal()) {
      UB_flag = true;
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    bool exact_flag = flags & BinOp::Flags::Exact;
    bool sdiv_ov = false;
    auto quotient = lhs_int->val.sdiv_ov(rhs_int->val, sdiv_ov);
    auto remainder = lhs_int->val.srem(rhs_int->val);
    if (sdiv_ov){
      UB_flag = true;
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }
    if (exact_flag && (remainder.getBoolValue() != false)){
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      v->setPoison(true);
      return v;
    }
    
    auto v = new ConcreteValInt(false, move(quotient));  
    return v;
  }

  ConcreteVal* ConcreteValInt::udiv(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags, bool& UB_flag) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);

    if (!rhs_int->getBoolVal()) {
      UB_flag = true;
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val.getBitWidth(),0));
    bool exact_flag = flags & BinOp::Flags::Exact;
    auto remainder = llvm::APInt();
    auto quotient = llvm::APInt();
    llvm::APInt::udivrem(lhs_int->val, rhs_int->val, quotient, remainder);
    if (exact_flag && (remainder.getBoolValue() != false)){
      v->setPoison(true);
      return v;
    }
    v->val = quotient;
    return v;
  }

  ConcreteVal* ConcreteValInt::srem(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags, bool& UB_flag) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);

    if (!rhs_int->getBoolVal()) {
      UB_flag = true;
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    bool sdiv_ov = false;
    auto quotient = lhs_int->val.sdiv_ov(rhs_int->val, sdiv_ov);
    auto remainder = lhs_int->val.srem(rhs_int->val);
    if (sdiv_ov){
      UB_flag = true;
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    auto v = new ConcreteValInt(false, move(remainder));  
    return v;
  }

  ConcreteVal* ConcreteValInt::urem(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags, bool& UB_flag) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);

    if (!rhs_int->getBoolVal()) {
      UB_flag = true;
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto remainder = lhs_int->val.urem(rhs_int->val);
    auto v = new ConcreteValInt(false, move(remainder));
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
  
  ConcreteVal* ConcreteValInt::andOp(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val));
    v->val&=rhs_int->val;
    return v;
  }

  ConcreteVal* ConcreteValInt::orOp(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val));
    v->val|=rhs_int->val;
    return v;
  }

  ConcreteVal* ConcreteValInt::xorOp(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val));
    v->val^=rhs_int->val;
    return v;
  }

  ConcreteVal* ConcreteValInt::abs(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs); //i1 is_int_min poison
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    if (lhs_int->val.isMinSignedValue()) {
      if (rhs_int->getBoolVal()) {
        auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
        return v;
      }
      else {
        auto v = new ConcreteValInt(*lhs_int);
        return v;
      }
    }
    auto abs_res = lhs_int->val.abs();
    auto v = new ConcreteValInt(false, move(abs_res));
    return v;
  }

  ConcreteVal* ConcreteValInt::lshr(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);// num
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);// shift amount
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    // first check if the shift amount is larger than the number's bitwidth -> return poison
    // the maximum bitwidth for an integer type is 2^23-1, so rhs cannot have a bitwidth larger than 2^23
    uint64_t max_shift = (1<<23);
    auto shift_amt_u_limit = rhs_int->val.getLimitedValue((1<<23));
    if ((shift_amt_u_limit == max_shift) ||
        (shift_amt_u_limit >= lhs_int->val.getBitWidth())){
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    // Need to handle this special case to safely subtract 1 from num_concrete 
    // when handling exact flag
    if (shift_amt_u_limit == 0){
      auto v = new ConcreteValInt(*lhs_int);
      return v;
    }

    bool exact_flag = flags & BinOp::Flags::Exact;
    // If any of the shifted bits is non-zero, we should return poison
    if (exact_flag){
      bool ov_flag_s = false;
      auto ap_1{llvm::APInt(lhs_int->val.getBitWidth(),1)};
      auto ap_mask{llvm::APInt(lhs_int->val.getBitWidth(),1)};
      ap_mask = ap_mask.shl(shift_amt_u_limit);
      ap_mask = ap_mask.ssub_ov(ap_1,ov_flag_s);
      ap_mask &= lhs_int->val;
      if (ap_mask.getBoolValue()){// exact shift returns poison
        auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
        return v;
      }
    }
    
    // at this point we can safely right shift our num by the amount 
    auto res = lhs_int->val.lshr(shift_amt_u_limit);
    auto v = new ConcreteValInt(false, move(res));
    return v;
  }

  ConcreteVal* ConcreteValInt::ashr(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);// num
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);// shift amount
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    // first check if the shift amount is larger than the number's bitwidth -> return poison
    // the maximum bitwidth for an integer type is 2^23-1, so rhs cannot have a bitwidth larger than 2^23
    uint64_t max_shift = (1<<23);
    auto shift_amt_u_limit = rhs_int->val.getLimitedValue((1<<23));
    if ((shift_amt_u_limit == max_shift) ||
        (shift_amt_u_limit >= lhs_int->val.getBitWidth())){
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    // Need to handle this special case to safely subtract 1 from num_concrete 
    // when handling exact flag
    if (shift_amt_u_limit == 0){
      auto v = new ConcreteValInt(*lhs_int);
      return v;
    }

    bool exact_flag = flags & BinOp::Flags::Exact;
    // If any of the shifted bits is non-zero, we should return poison
    if (exact_flag){
      bool ov_flag_s = false;
      auto ap_1{llvm::APInt(lhs_int->val.getBitWidth(),1)};
      auto ap_mask{llvm::APInt(lhs_int->val.getBitWidth(),1)};
      ap_mask = ap_mask.shl(shift_amt_u_limit);
      ap_mask = ap_mask.ssub_ov(ap_1,ov_flag_s);
      ap_mask &= lhs_int->val;
      if (ap_mask.getBoolValue()){// exact shift returns poison
        auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
        return v;
      }
    }
    
    // at this point we can safely right shift our num by the amount 
    auto res = lhs_int->val.ashr(shift_amt_u_limit);
    auto v = new ConcreteValInt(false, move(res));
    return v;
  }

  ConcreteVal* ConcreteValInt::shl(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);// num
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);// shift amount
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;
    
    // first check if the shift amount is larger than the number's bitwidth -> return poison
    // the maximum bitwidth for an integer type is 2^23-1, so rhs cannot have a bitwidth larger than 2^23
    uint64_t max_shift = (1<<23);
    auto shift_amt_u_limit = rhs_int->val.getLimitedValue((1<<23));
    if ((shift_amt_u_limit == max_shift) ||
        (shift_amt_u_limit >= lhs_int->val.getBitWidth())){
      auto v = new ConcreteValInt(true, llvm::APInt(lhs_int->val.getBitWidth(),0));
      return v;
    }

    // Need to handle this special case to safely subtract 1 from num_concrete 
    // when handling exact flag
    if (shift_amt_u_limit == 0){
      auto v = new ConcreteValInt(*lhs_int);
      return v;
    }

    bool nsw_flag = flags & BinOp::Flags::NSW;
    bool nuw_flag = flags & BinOp::Flags::NUW;
    auto res = lhs_int->val.shl(shift_amt_u_limit);

    if (!nuw_flag && !nsw_flag){
      auto v = new ConcreteValInt(false, move(res));
      return v;
    }
    auto v = new ConcreteValInt(false, llvm::APInt(lhs_int->val.getBitWidth(), 0));
    if (nuw_flag){
      auto lhs_copy {lhs_int->val};
      for (unsigned i=0; i < shift_amt_u_limit; ++i){
        if (lhs_copy.isSignBitSet()){
          v->setPoison(true);
          return v;
        }
        lhs_copy<<=1;
      }
    }
    if (nsw_flag){
      bool res_sign_bit = res.isSignBitSet();
      auto lhs_copy {lhs_int->val};
      for (unsigned i=0; i < shift_amt_u_limit; ++i){
        if (lhs_copy.isSignBitSet() != res_sign_bit){
          v->setPoison(true);
          return v;
        }
        lhs_copy<<=1;
      }
    }
    v->val = move(res);
    return v;
  }

  ConcreteVal* ConcreteValInt::cttz(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);// num
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);// is zero undef
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto lhs_bitwidth = lhs_int->val.getBitWidth();
    auto v = new ConcreteValInt(false,llvm::APInt(lhs_bitwidth, 0));
    if (rhs_int->getBoolVal() && (lhs_int->getBoolVal() == false) ){
      v->setUndef();
      return v;
    }

    uint64_t cnt = 0;
    for (unsigned i = 0; i < lhs_bitwidth ; ++i){
        if (lhs_int->val.extractBits(1,i).getBoolValue()){
          break;
        }
        cnt+=1;
    }
    auto int_res = llvm::APInt(lhs_bitwidth, cnt);
    v->val = move(int_res);
    return v;
  }

  ConcreteVal* ConcreteValInt::ctlz(ConcreteVal* lhs, ConcreteVal* rhs, unsigned flags) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);// num
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);// is zero undef
    assert(lhs_int && rhs_int);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto lhs_bitwidth = lhs_int->val.getBitWidth();
    auto v = new ConcreteValInt(false,llvm::APInt(lhs_bitwidth, 0));
    if (rhs_int->getBoolVal() && (lhs_int->getBoolVal() == false) ){
      v->setUndef();
      return v;
    }

    uint64_t cnt = 0;
    for (int i = lhs_bitwidth - 1; i >= 0 ; --i){
        if (lhs_int->val.extractBits(1,i).getBoolValue()){
          break;
        }
        cnt+=1;
    }
    auto int_res = llvm::APInt(lhs_bitwidth, cnt);
    v->val = move(int_res);
    return v;
  }

  ConcreteVal *ConcreteValInt::arithOverflow(ConcreteVal *lhs, ConcreteVal *rhs,
                                             unsigned opcode) {
    auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs);
    auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs);
    assert(lhs_int && rhs_int);
    vector<shared_ptr<ConcreteVal>> elements;
    if (lhs->isPoison() || rhs->isPoison()) {

      elements.push_back(shared_ptr<ConcreteVal>(new ConcreteValInt(
          true, llvm::APInt(lhs_int->val.getBitWidth(), 0))));
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(true, llvm::APInt(1, 0))));

      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    }
    bool ov_flag;
    if (opcode == BinOp::SAdd_Overflow) {
      auto ap_sum_s = lhs_int->val.sadd_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_sum_s))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    } else if (opcode == BinOp::UAdd_Overflow) {
      auto ap_sum_u = lhs_int->val.uadd_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_sum_u))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    } else if (opcode == BinOp::SSub_Overflow) {
      auto ap_diff_s = lhs_int->val.ssub_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_diff_s))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    } else if (opcode == BinOp::USub_Overflow) {
      auto ap_diff_u = lhs_int->val.usub_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_diff_u))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v; 
    } else if (opcode == BinOp::SMul_Overflow) {
      auto ap_mul_s = lhs_int->val.smul_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_mul_s))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    } else if (opcode == BinOp::UMul_Overflow) {
      auto ap_mul_u = lhs_int->val.umul_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_mul_u))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    } else if (opcode == BinOp::SMul_Overflow) {
      auto ap_mul_u = lhs_int->val.smul_ov(rhs_int->val, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_mul_u))));
      auto ov_int = llvm::APInt(1, ov_flag);
      elements.push_back(
          shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
      auto v = new ConcreteValAggregate(false, move(elements));
      return v;
    } 
    
    UNREACHABLE();
  }

  ConcreteVal* ConcreteValInt::ctpop(ConcreteVal* op) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);// num
    assert(op_int);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto op_bitwidth = op_int->val.getBitWidth();
    uint64_t cnt = 0;
    for (unsigned i = 0; i < op_bitwidth; ++i){
      if (op_int->val.extractBits(1,i).getBoolValue()){
        cnt +=1;
      }
    }
    auto v = new ConcreteValInt(false, llvm::APInt(op_bitwidth,cnt));  
    return v;
    
  }

  ConcreteVal* ConcreteValInt::bitreverse(ConcreteVal* op) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);// num
    assert(op_int);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto v = new ConcreteValInt(false, op_int->val.reverseBits());  
    return v;
    
  }

  ConcreteVal* ConcreteValInt::bswap(ConcreteVal* op) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);// num
    assert(op_int);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto op_bitwidth = op_int->val.getBitWidth();
    // the assertion in APInt's byteSwap does not match llvm's semantics
    assert(op_bitwidth >= 16 && op_bitwidth % 16 == 0);
    auto v = new ConcreteValInt(false, op_int->val.byteSwap());  
    return v;
    
  }

  ConcreteVal* ConcreteValInt::iTrunc(ConcreteVal* op, unsigned tgt_bitwidth) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);
    assert(op_int);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    assert(op_int->val.getBitWidth() > tgt_bitwidth);// Already causes an error in APInt::trunc
    auto res = op_int->val.trunc(tgt_bitwidth);
    auto v = new ConcreteValInt(false, move(res));
    return v;
  }

  ConcreteVal* ConcreteValInt::zext(ConcreteVal* op, unsigned tgt_bitwidth) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);
    assert(op_int);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    assert(op_int->val.getBitWidth() < tgt_bitwidth);// Already causes an error in APInt::zext
    auto res = op_int->val.zext(tgt_bitwidth);
    auto v = new ConcreteValInt(false, move(res));
    return v;
  }

  ConcreteVal* ConcreteValInt::sext(ConcreteVal* op, unsigned tgt_bitwidth) {
    auto op_int = dynamic_cast<ConcreteValInt *>(op);
    assert(op_int);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    assert(op_int->val.getBitWidth() < tgt_bitwidth);// Already causes an error in APInt::sext
    auto res = op_int->val.sext(tgt_bitwidth);
    auto v = new ConcreteValInt(false, move(res));
    return v;
  }

  shared_ptr<ConcreteVal> ConcreteValInt::select(ConcreteVal *cond,
                                                 shared_ptr<ConcreteVal> &a,
                                                 shared_ptr<ConcreteVal> &b) {
    auto cond_int = dynamic_cast<ConcreteValInt *>(cond);
    assert(cond_int);
    // The result is only poison if cond or the selected value is poison; the
    // other value doesn't matter.
    auto poison_res = evalPoison(cond_int);
    if (poison_res)
      return shared_ptr<ConcreteVal>(poison_res);
    return cond_int->getBoolVal() ? a : b;
  }

  ConcreteVal* ConcreteValInt::icmp(ConcreteVal* a, ConcreteVal* b, unsigned cond) {
    auto a_int = dynamic_cast<ConcreteValInt *>(a);
    auto b_int = dynamic_cast<ConcreteValInt *>(b);
    assert(a_int && b_int);
    auto poison_res = evalPoison(a_int, b_int);
    if (poison_res) 
      return poison_res;

    bool icmp_res = false;
    switch (cond) {
      case ICmp::Cond::EQ:
        icmp_res = a_int->val.eq(b_int->val);
        break;
      case ICmp::Cond::NE:  
        icmp_res = a_int->val.ne(b_int->val);
        break;
      case ICmp::Cond::SLE: 
        icmp_res = a_int->val.sle(b_int->val);
        break;
      case ICmp::Cond::SLT: 
        icmp_res = a_int->val.slt(b_int->val);
        break;
      case ICmp::Cond::SGE: 
        icmp_res = a_int->val.sge(b_int->val);
        break;
      case ICmp::Cond::SGT: 
        icmp_res = a_int->val.sgt(b_int->val);
        break;
      case ICmp::Cond::ULE:
        icmp_res = a_int->val.ule(b_int->val);
        break;
      case ICmp::Cond::ULT: 
        icmp_res = a_int->val.ult(b_int->val);
        break;
      case ICmp::Cond::UGE: 
        icmp_res = a_int->val.uge(b_int->val);
        break;
      case ICmp::Cond::UGT: 
        icmp_res = a_int->val.ugt(b_int->val);
        break;
      case ICmp::Cond::Any:
          UNREACHABLE();
    }

    if (icmp_res){
      auto v = new ConcreteValInt(false, llvm::APInt(1,1));
      return v;
    }
    else{
      auto v = new ConcreteValInt(false, llvm::APInt(1,0));
      return v;
    }
    
    UNREACHABLE();
  }

  ConcreteVal *ConcreteValInt::fshl(ConcreteVal *a, ConcreteVal *b,
                                    ConcreteVal *c) {
    auto a_int = dynamic_cast<ConcreteValInt *>(a);
    auto b_int = dynamic_cast<ConcreteValInt *>(b);
    auto c_int = dynamic_cast<ConcreteValInt *>(c);
    assert(a_int && b_int && c_int);
    auto op_bitwidth = c_int->val.getBitWidth();
    auto poison_res = evalPoison(a, b, c);

    if (poison_res)
      return poison_res;

    auto shift_amt = c_int->val.urem(op_bitwidth);
    auto tmp_val = a_int->val.concat(b_int->val).shl(shift_amt);
    auto res_val =
        tmp_val.extractBits(op_bitwidth, tmp_val.getBitWidth() - op_bitwidth);
    auto v = new ConcreteValInt(false, move(res_val));
    return v;
  }

  ConcreteVal *ConcreteValInt::fshr(ConcreteVal *a, ConcreteVal *b,
                                    ConcreteVal *c) {
    auto a_int = dynamic_cast<ConcreteValInt *>(a);
    auto b_int = dynamic_cast<ConcreteValInt *>(b);
    auto c_int = dynamic_cast<ConcreteValInt *>(c);
    assert(a_int && b_int && c_int);
    auto op_bitwidth = c_int->val.getBitWidth();
    auto poison_res = evalPoison(a, b, c);

    if (poison_res)
      return poison_res;

    auto shift_amt = c_int->val.urem(op_bitwidth);
    auto tmp_val = a_int->val.concat(b_int->val).lshr(shift_amt);
    auto res_val = tmp_val.extractBits(op_bitwidth, 0);
    auto v = new ConcreteValInt(false, move(res_val));
    return v;
  }

  ConcreteValFloat::ConcreteValFloat(bool poison, llvm::APFloat &&val)
  : ConcreteVal(poison), val(move(val)) {

  }

  llvm::APFloat ConcreteValFloat::getVal() const {
    return val;
  }

  ConcreteVal* ConcreteValFloat::evalPoison(ConcreteVal* op1, ConcreteVal* op2, ConcreteVal* op3) {
    auto op1_float = dynamic_cast<ConcreteValFloat *>(op1);

    if (op1->isPoison() || op2->isPoison() || op3->isPoison()) {
      auto v = new ConcreteValFloat(true, llvm::APFloat(op1_float->val));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValFloat::evalPoison(ConcreteVal* lhs, ConcreteVal* rhs) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
  
    if (lhs->isPoison() || rhs->isPoison()) {
      auto v = new ConcreteValFloat(true, llvm::APFloat(lhs_float->val));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValFloat::evalPoison(ConcreteVal* op) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
  
    if (op->isPoison()) {
      auto v = new ConcreteValFloat(true, llvm::APFloat(op_float->val));
      return v;
    }
    return nullptr;
  }

  ConcreteVal* ConcreteValFloat::evalFmath(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
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

  ConcreteVal* ConcreteValFloat::evalFmath(ConcreteVal* op, IR::FastMathFlags fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
  
    if (fmath.isNNan()){
      if (op_float->val.isNaN()){
        auto v = new ConcreteValFloat(true, llvm::APFloat(op_float->val));
        return v;
      }
    }

    if (fmath.isNInf()){ 
      if (op_float->val.isInfinity()){
        auto v = new ConcreteValFloat(true, llvm::APFloat(op_float->val));
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

  void ConcreteValFloat::setVal(llvm::APFloat& v) {
    val = v;
  }

  ConcreteVal* ConcreteValFloat::fadd(ConcreteVal* lhs, ConcreteVal* rhs, IR::FastMathFlags fmath) {
    auto lhs_float = dynamic_cast<ConcreteValFloat *>(lhs);
    auto rhs_float = dynamic_cast<ConcreteValFloat *>(rhs);
    assert(lhs_float && rhs_float);
    auto poison_res = evalPoison(lhs, rhs);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(lhs, rhs, fmath);
    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    v->val.add(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    // assert(status == llvm::APFloatBase::opOK);
    unOPEvalFmath(v, fmath);
    return v;
    
  }

  ConcreteVal* ConcreteValFloat::fabs(ConcreteVal* op, IR::FastMathFlags fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.clearSign();
    // CHECK can this every be true?
    unOPEvalFmath(v, fmath);
    return v;
  }

  ConcreteVal* ConcreteValFloat::fneg(ConcreteVal* op, const IR::FastMathFlags& fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.changeSign();
    // CHECK can this every be true?
    unOPEvalFmath(v, fmath);
    return v;
  }

  ConcreteVal* ConcreteValFloat::ceil(ConcreteVal* op, const IR::FastMathFlags& fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.roundToIntegral(llvm::RoundingMode::TowardPositive);
    // CHECK can this every be true?
    unOPEvalFmath(v, fmath);
    return v;
  }

  ConcreteVal* ConcreteValFloat::floor(ConcreteVal* op, const IR::FastMathFlags& fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.roundToIntegral(llvm::RoundingMode::TowardNegative);
    // CHECK can this ever be true?
    unOPEvalFmath(v, fmath);
    return v;   
  }

  ConcreteVal* ConcreteValFloat::round(ConcreteVal* op, const IR::FastMathFlags& fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.roundToIntegral(llvm::RoundingMode::NearestTiesToAway);
    // CHECK can this every be true?
    unOPEvalFmath(v, fmath);
    return v;   
  }

    ConcreteVal* ConcreteValFloat::roundEven(ConcreteVal* op, const IR::FastMathFlags& fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.roundToIntegral(llvm::RoundingMode::NearestTiesToEven);
    // CHECK can this every be true?
    unOPEvalFmath(v, fmath);
    return v;   
  }

  ConcreteVal* ConcreteValFloat::trunc(ConcreteVal* op, const IR::FastMathFlags& fmath) {
    auto op_float = dynamic_cast<ConcreteValFloat *>(op);
    assert(op_float);
    auto poison_res = evalPoison(op);
    if (poison_res) 
      return poison_res;

    auto fmath_input_res = evalFmath(op, fmath);
    if (fmath_input_res)
      return fmath_input_res;
    
    auto v = new ConcreteValFloat(false, llvm::APFloat(op_float->val));
    v->val.roundToIntegral(llvm::RoundingMode::TowardZero);
    // CHECK can this ever be true?
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

    auto fmath_input_res = evalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    v->val.subtract(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    //assert(status == llvm::APFloatBase::opOK);
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

    auto fmath_input_res = evalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    v->val.multiply(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    //assert(status == llvm::APFloatBase::opOK);
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

    auto fmath_input_res = evalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    v->val.divide(rhs_float->val, llvm::APFloatBase::rmNearestTiesToEven);
    //assert(status == llvm::APFloatBase::opOK);
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

    auto fmath_input_res = evalFmath(lhs, rhs, fmath);

    if (fmath_input_res)
      return fmath_input_res;

    auto v = new ConcreteValFloat(false, llvm::APFloat(lhs_float->val));
    v->val.mod(rhs_float->val);
    //assert(status == llvm::APFloatBase::opOK);
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

  ConcreteVal* ConcreteValFloat::fma(ConcreteVal* a, ConcreteVal* b, ConcreteVal* c) {
    auto a_float = dynamic_cast<ConcreteValFloat *>(a);
    auto b_float = dynamic_cast<ConcreteValFloat *>(b);
    auto c_float = dynamic_cast<ConcreteValFloat *>(c);
    assert(a_float && b_float && c_float);
    auto poison_res = evalPoison(a, b, c);

    if (poison_res) 
      return poison_res;

    auto v = new ConcreteValFloat(*a_float);
    // Should we check the opStatus from the fusedMultipyAdd
    v->val.fusedMultiplyAdd(b_float->val, c_float->val, llvm::APFloatBase::rmNearestTiesToEven);
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

  ConcreteValAggregate::ConcreteValAggregate(
      bool poison, std::vector<shared_ptr<ConcreteVal>> &&elements)
      : ConcreteVal(poison), elements(move(elements)) {}

  const vector<shared_ptr<ConcreteVal>> &ConcreteValAggregate::getVal() const {
    return elements;
  }

  ConcreteValAggregate::~ConcreteValAggregate() {
  }

  void ConcreteValAggregate::print() {
    cout << "<" << '\n';
    for (auto &elem : elements) {
      elem->print();
    }
    cout << ">" << '\n';
  }

  ConcreteVal *ConcreteValAggregate::evalBinOp(ConcreteVal *lhs,
                                               ConcreteVal *rhs,
                                               unsigned opcode, unsigned flags,
                                               Interpreter &interpreter) {
    auto lhs_vect = dynamic_cast<ConcreteValAggregate *>(lhs);
    auto rhs_vect = dynamic_cast<ConcreteValAggregate *>(rhs);
    assert(lhs_vect && rhs_vect);
    assert(lhs_vect->elements.size() == rhs_vect->elements.size());
    vector<shared_ptr<ConcreteVal>> elements;

    if (opcode == BinOp::Op::SAdd_Overflow ||
        opcode == BinOp::Op::UAdd_Overflow ||
        opcode == BinOp::Op::SSub_Overflow ||
        opcode == BinOp::Op::USub_Overflow || 
        opcode == BinOp::Op::SMul_Overflow ||
        opcode == BinOp::Op::UMul_Overflow) {
      auto res = arithOverflow(lhs_vect, rhs_vect, opcode);
      // if (res)
      //   res->print(); // TEMP
      return res;
    }

    for (unsigned i = 0; i < lhs_vect->elements.size(); i++) {
      auto lhs_elem = lhs_vect->elements[i];
      auto rhs_elem = rhs_vect->elements[i];
      auto int_elem = dynamic_cast<ConcreteValInt *>(lhs_elem.get());
      assert(int_elem);
      ConcreteVal *res_elem = nullptr;
      switch (opcode) {
      case BinOp::Op::Add:
        res_elem = ConcreteValInt::add(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::Sub:
        res_elem = ConcreteValInt::sub(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::Mul:
        res_elem = ConcreteValInt::mul(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::SDiv:
        res_elem = ConcreteValInt::sdiv(lhs_elem.get(), rhs_elem.get(), flags,
                                        interpreter.UB_flag);
        break;
      case BinOp::Op::UDiv:
        res_elem = ConcreteValInt::udiv(lhs_elem.get(), rhs_elem.get(), flags,
                                        interpreter.UB_flag);
        break;
      case BinOp::Op::SRem:
        res_elem = ConcreteValInt::srem(lhs_elem.get(), rhs_elem.get(), flags,
                                        interpreter.UB_flag);
        break;
      case BinOp::Op::URem:
        res_elem = ConcreteValInt::urem(lhs_elem.get(), rhs_elem.get(), flags,
                                        interpreter.UB_flag);
        break;
      case BinOp::Op::SAdd_Sat:
        res_elem =
            ConcreteValInt::sAddSat(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::UAdd_Sat:
        res_elem =
            ConcreteValInt::uAddSat(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::SSub_Sat:
        res_elem =
            ConcreteValInt::sSubSat(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::USub_Sat:
        res_elem =
            ConcreteValInt::uSubSat(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::SShl_Sat:
        res_elem =
            ConcreteValInt::sShlSat(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::UShl_Sat:
        res_elem =
            ConcreteValInt::uShlSat(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::And:
        res_elem = ConcreteValInt::andOp(lhs_elem.get(), rhs_elem.get());
        break;
      case BinOp::Op::Or:
        res_elem = ConcreteValInt::orOp(lhs_elem.get(), rhs_elem.get());
        break;
      case BinOp::Op::Xor:
        res_elem = ConcreteValInt::xorOp(lhs_elem.get(), rhs_elem.get());
        break;
      case BinOp::Op::Abs:
        res_elem = ConcreteValInt::abs(lhs_elem.get(), rhs_elem.get());
        break;
      case BinOp::Op::LShr:
        res_elem = ConcreteValInt::lshr(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::AShr:
        res_elem = ConcreteValInt::ashr(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::Shl:
        res_elem = ConcreteValInt::shl(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::Cttz:
        res_elem = ConcreteValInt::cttz(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      case BinOp::Op::Ctlz:
        res_elem = ConcreteValInt::ctlz(lhs_elem.get(), rhs_elem.get(), flags);
        break;
      default:
        res_elem = nullptr;
        break;
      }

      if (!res_elem) {
        return nullptr;
      }

      elements.push_back(shared_ptr<ConcreteVal>(res_elem));
    }
    auto res = new ConcreteValAggregate(false, move(elements));
    res->print(); // TEMP
    return res;
  }

  ConcreteVal *ConcreteValAggregate::evalFPBinOp(ConcreteVal *lhs,
                                                 ConcreteVal *rhs,
                                                 unsigned opcode, 
                                                 IR::FastMathFlags fmath,
                                                 Interpreter &interpreter) {
    auto lhs_vect = dynamic_cast<ConcreteValAggregate *>(lhs);
    auto rhs_vect = dynamic_cast<ConcreteValAggregate *>(rhs);
    assert(lhs_vect && rhs_vect);
    assert(lhs_vect->elements.size() == rhs_vect->elements.size());
    vector<shared_ptr<ConcreteVal>> elements;

    for (unsigned i = 0; i < lhs_vect->elements.size(); i++) {
      auto lhs_elem = lhs_vect->elements[i];
      auto rhs_elem = rhs_vect->elements[i];
      auto float_elem = dynamic_cast<ConcreteValFloat *>(lhs_elem.get());
      assert(float_elem);
      ConcreteVal *res_elem = nullptr;
      switch (opcode) {
      case FpBinOp::Op::FAdd:
        res_elem =
            ConcreteValFloat::fadd(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FSub:
        res_elem =
            ConcreteValFloat::fsub(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FMul:
        res_elem =
            ConcreteValFloat::fmul(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FDiv:
        res_elem =
            ConcreteValFloat::fdiv(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FRem:
        res_elem =
            ConcreteValFloat::frem(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FMax:
        res_elem =
            ConcreteValFloat::fmax(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FMin:
        res_elem =
            ConcreteValFloat::fmin(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FMaximum:
        res_elem =
            ConcreteValFloat::fmaximum(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      case FpBinOp::Op::FMinimum:
        res_elem =
            ConcreteValFloat::fminimum(lhs_elem.get(), rhs_elem.get(), fmath);
        break;
      default:
        res_elem = nullptr;
        break;
      }

      if (!res_elem) {
        return nullptr;
      }

      elements.push_back(shared_ptr<ConcreteVal>(res_elem));
    }
    auto res = new ConcreteValAggregate(false, move(elements));
    res->print(); // TEMP
    return res;
  }

  ConcreteVal *ConcreteValAggregate::icmp(ConcreteVal *a, ConcreteVal *b,
                                          unsigned cond, unsigned pcmode,
                                          Interpreter &interpreter) {
    auto lhs_vect = dynamic_cast<ConcreteValAggregate *>(a);
    auto rhs_vect = dynamic_cast<ConcreteValAggregate *>(b);
    assert(lhs_vect && rhs_vect);
    assert(lhs_vect->elements.size() > 0 &&
           lhs_vect->elements.size() == rhs_vect->elements.size());
    auto first_elem = lhs_vect->elements[0].get();
    auto int_elem = dynamic_cast<ConcreteValInt *>(first_elem);
    auto ptr_elem = dynamic_cast<ConcreteValPointer *>(first_elem);
    assert((int_elem || ptr_elem) &&
           "icmp vector elements must either have int or pointer type");
    vector<shared_ptr<ConcreteVal>> elements;
    bool all_poison = true;
    for (unsigned i = 0; i < lhs_vect->elements.size(); i++) {
      auto lhs_elem = lhs_vect->elements[i];
      auto rhs_elem = rhs_vect->elements[i];
      if (int_elem) {
        auto res_elem =
            ConcreteValInt::icmp(lhs_elem.get(), rhs_elem.get(), cond);
        elements.push_back(shared_ptr<ConcreteVal>(res_elem));
        if (!res_elem->isPoison()) {
          all_poison = false;
        }
      } else { // TODO: add support for pointer comparison
        interpreter.setUnsupported(
            "icmp vector pointer type not supported yet");
        return nullptr;
      }
    }

    return new ConcreteValAggregate(all_poison, move(elements));
  }

  ConcreteValAggregate *
  ConcreteValAggregate::arithOverflow(ConcreteValAggregate *lhs_vect,
                                      ConcreteValAggregate *rhs_vect,
                                      unsigned opcode) {

    vector<shared_ptr<ConcreteVal>> res_elements;
    vector<shared_ptr<ConcreteVal>> ov_elements;
    for (unsigned i = 0; i < lhs_vect->elements.size(); i++) {
      auto lhs_elem = lhs_vect->elements[i];
      auto rhs_elem = rhs_vect->elements[i];
      auto lhs_int = dynamic_cast<ConcreteValInt *>(lhs_elem.get());
      auto rhs_int = dynamic_cast<ConcreteValInt *>(rhs_elem.get());
      assert(lhs_int && rhs_int && "sAddOverflow vector elements must have int type");

      if (lhs_int->isPoison() || rhs_int->isPoison()) {

        res_elements.push_back(shared_ptr<ConcreteVal>(new ConcreteValInt(
            true, llvm::APInt(lhs_int->getVal().getBitWidth(), 0))));
        ov_elements.push_back(shared_ptr<ConcreteVal>(
            new ConcreteValInt(true, llvm::APInt(1, 0))));
        continue;
      }

      bool ov_flag;
      llvm::APInt ap_res;
      if (opcode == BinOp::SAdd_Overflow) {
        ap_res = lhs_int->getVal().sadd_ov(rhs_int->getVal(), ov_flag);
      } else if (opcode == BinOp::UAdd_Overflow) {
        ap_res = lhs_int->getVal().uadd_ov(rhs_int->getVal(), ov_flag);
      } else if (opcode == BinOp::SSub_Overflow) {
        ap_res = lhs_int->getVal().ssub_ov(rhs_int->getVal(), ov_flag);
      } else if (opcode == BinOp::USub_Overflow) {
        ap_res = lhs_int->getVal().usub_ov(rhs_int->getVal(), ov_flag);
      } else if (opcode == BinOp::SMul_Overflow) {
        ap_res = lhs_int->getVal().smul_ov(rhs_int->getVal(), ov_flag);
      } else if (opcode == BinOp::UMul_Overflow) {
        ap_res = lhs_int->getVal().umul_ov(rhs_int->getVal(), ov_flag);
      } else {
        assert(false && "unsupported arithmetic overflow operation");
      }
      res_elements.push_back(shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ap_res))));
      auto ov_int = llvm::APInt(1, ov_flag);
      ov_elements.push_back(shared_ptr<ConcreteVal>(new ConcreteValInt(false, move(ov_int))));
    }

    auto res_vect = new ConcreteValAggregate(false, move(res_elements));
    auto ov_vect = new ConcreteValAggregate(false, move(ov_elements));
    vector<shared_ptr<ConcreteVal>> v_vect;
    v_vect.push_back(shared_ptr<ConcreteVal>(res_vect));
    v_vect.push_back(shared_ptr<ConcreteVal>(ov_vect));
    auto v = new ConcreteValAggregate(false, move(v_vect));
    return v;
  }

  ConcreteValPointer::ConcreteValPointer()
      : ConcreteVal(false), bid(0), offset(0) {}

  ConcreteValPointer::ConcreteValPointer(bool poison, unsigned bid,
                                         std::int64_t offset, bool is_local)
      : ConcreteVal(poison), bid(bid), offset(offset), is_local(is_local) {}

  unsigned ConcreteValPointer::getBid() const {
    return bid;
  }

  bool ConcreteValPointer::getIsLocal() const {
    return is_local;
  }

  std::int64_t ConcreteValPointer::getOffset() const {
    return offset;
  }

  void ConcreteValPointer::setOffset(std::int64_t offset) {
    this->offset = offset;
  }

  void ConcreteValPointer::print() {
    cout << "pointer(poison=" << isPoison() << ", block_id=" << bid << ", offset=" << offset << ", is_local=" << is_local << ")\n";
  }

  ConcreteVal *ConcreteValPointer::evalPoison(ConcreteVal *lhs,
                                              ConcreteVal *rhs) {
    
    if (lhs->isPoison() || rhs->isPoison()) {
      auto v = new ConcreteValInt(true, llvm::APInt(1,1));
      v->setPoison(true);
      return v;
    }
    return nullptr;
  }

  bool ConcreteValPointer::icmp_cmp(llvm::APInt &lhs, llvm::APInt &rhs,
                                    unsigned cond) {
    bool icmp_res = false;
    switch (cond) {
    case ICmp::Cond::EQ:
      icmp_res = lhs.eq(rhs);
      break;
    case ICmp::Cond::NE:
      icmp_res = lhs.ne(rhs);
      break;
    case ICmp::Cond::SLE:
      icmp_res = lhs.sle(rhs);
      break;
    case ICmp::Cond::SLT:
      icmp_res = lhs.slt(rhs);
      break;
    case ICmp::Cond::SGE:
      icmp_res = lhs.sge(rhs);
      break;
    case ICmp::Cond::SGT:
      icmp_res = lhs.sgt(rhs);
      break;
    case ICmp::Cond::ULE:
      icmp_res = lhs.ule(rhs);
      break;
    case ICmp::Cond::ULT:
      icmp_res = lhs.ult(rhs);
      break;
    case ICmp::Cond::UGE:
      icmp_res = lhs.uge(rhs);
      break;
    case ICmp::Cond::UGT:
      icmp_res = lhs.ugt(rhs);
      break;
    case ICmp::Cond::Any:
      UNREACHABLE();
    }
    return icmp_res;

    UNREACHABLE();
  }

  static uint64_t compute_ptr_address(ConcreteValPointer *ptr, bool &ov,
                                      Interpreter &interpreter) {
    auto &ptr_block = interpreter.getBlock(ptr->getBid(), ptr->getIsLocal());
    uint64_t addr = 0;
    ov = __builtin_uaddl_overflow(ptr_block.address, ptr->getOffset(), &addr);
    return addr;
  }

  ConcreteVal *ConcreteValPointer::icmp(ConcreteVal *a, ConcreteVal *b,
                                        unsigned cond, unsigned pcmode,
                                        Interpreter &interpreter) {
    auto a_ptr = dynamic_cast<ConcreteValPointer *>(a);
    auto b_ptr = dynamic_cast<ConcreteValPointer *>(b);
    assert(a_ptr && b_ptr);
    auto poison_res = evalPoison(a_ptr, b_ptr);
    if (poison_res)
      return poison_res;

    bool lhs_ov = false;
    bool rhs_ov = false;
    bool icmp_res = false;

    if (pcmode == ICmp::PtrCmpMode::INTEGRAL) {
      auto lhs_addr = compute_ptr_address(a_ptr, lhs_ov, interpreter);
      auto rhs_addr = compute_ptr_address(b_ptr, rhs_ov, interpreter);
      if (lhs_ov || rhs_ov) {
        interpreter.UB_flag = true;
        return nullptr;
      } else {
        auto lhs_val = llvm::APInt(64, lhs_addr);
        auto rhs_val = llvm::APInt(64, rhs_addr);
        icmp_res = icmp_cmp(lhs_val, rhs_val, cond);
      }
    } else if (pcmode == ICmp::PtrCmpMode::PROVENANCE) {
      assert(cond == ICmp::Cond::EQ || cond == ICmp::Cond::NE);
      icmp_res = cond == ICmp::Cond::EQ ? *a_ptr == *b_ptr : *a_ptr != *b_ptr;
    } else if (pcmode == ICmp::PtrCmpMode::OFFSETONLY) {
      auto lhs_val = llvm::APInt(64, a_ptr->getOffset());
      auto rhs_val = llvm::APInt(64, a_ptr->getOffset());
      icmp_res = icmp_cmp(lhs_val, rhs_val, cond);
    } else {
      assert(false && "icmp unsupported pointer comparison mode");
    }
    cout << "icmp res = " << icmp_res << '\n';
    if (icmp_res) {
      auto v = new ConcreteValInt(false, llvm::APInt(1, 1));
      return v;
    } else {
      auto v = new ConcreteValInt(false, llvm::APInt(1, 0));
      return v;
    }

    UNREACHABLE();
  }
}

