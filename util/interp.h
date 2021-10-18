#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace IR {
class BasicBlock;
class Function;
class Input;
class Value;
} // namespace IR

namespace util {

class ConcreteVal;

struct ConcreteByte {
  bool is_pointer;
  // for now, for non pointer bytes, treat the entire byte as an i8 for poison
  // purposes
  std::variant<ConcreteValPointer, ConcreteValInt> value;
  int pointer_byte_offset;

  ConcreteByte()
      : is_pointer(false), value(ConcreteValInt(true, llvm::APInt(8, 0))),
        pointer_byte_offset(0) {}

  ConcreteByte(ConcreteValPointer &&_value)
      : is_pointer(true), value(std::move(_value)), pointer_byte_offset(0) {}

  ConcreteByte(ConcreteValInt &&_value)
      : is_pointer(false), value(std::move(_value)), pointer_byte_offset(0) {}

  auto &intValue() {
    assert(!is_pointer);
    return get<ConcreteValInt>(value);
  }

  auto &pointerValue() {
    assert(is_pointer);
    return get<ConcreteValPointer>(value);
  }

  bool operator==(ConcreteByte &rhs) {
    if (is_pointer != rhs.is_pointer)
      return false;
    if (is_pointer) {
      if (pointer_byte_offset != rhs.pointer_byte_offset)
        return false;
      return (pointerValue() == rhs.pointerValue());
    } else {
      return (intValue() == rhs.intValue());
    }
    UNREACHABLE();
  }

  void print(std::ostream &os) const {
    if (is_pointer) {
      auto is_poison = get<ConcreteValPointer>(value).isPoison();
      auto nonpoison_mask = is_poison ? 0 : 255;
      auto ptr_bid = get<ConcreteValPointer>(value).getBid();
      auto ptr_offset = get<ConcreteValPointer>(value).getOffset();
      os << nonpoison_mask << ",[" << ptr_bid << "," << ptr_offset << ","
         << pointer_byte_offset << "]";
    } else {
      auto is_poison = get<ConcreteValInt>(value).isPoison();
      auto nonpoison_mask = is_poison ? 0 : 255;
      llvm::SmallString<40> U;
      get<ConcreteValInt>(value).getVal().toStringUnsigned(U);
      os << nonpoison_mask << "," << U.c_str() << "]";
    }
  }
};

struct ConcreteBlock {
  uint64_t size;
  uint64_t address;
  uint64_t align;
  std::map<uint64_t, ConcreteByte> bytes;
  ConcreteByte default_byte;

  ConcreteBlock() {}

  void print(std::ostream &os) const {
    os << "size:" << size << ", align:" << align;
    if (size == 0)
      assert(bytes.empty());
    else {
      os << ", bytes:[";

      for (auto &[index, cb] : bytes) {
        os << "[" << index << ",";
        cb.print(os);
      }
      os << "]";
    }
  }

  ConcreteByte &getByte(uint64_t index) {
    if (!bytes.contains(index))
      return default_byte;
    return bytes[index];
  }

  bool operator==(ConcreteBlock &rhs) {
    std::cout << "operator== concreteBlock\n";
    if ((size != rhs.size) || (address != rhs.address) || (align != rhs.align))
      return false;
    if (bytes.size() != rhs.bytes.size())
      return false;
    for (auto &[index, cb] : bytes) {
      if (!rhs.bytes.contains(index))
        return false;
      if (cb != rhs.bytes[index]) {
        return false;
      }
    }
    return true;
  }
};

class Interpreter {
public:
  Interpreter();
  virtual ~Interpreter();
  void start(IR::Function &f);
  void step();
  void run(unsigned instr_limit = 100);
  void setUnsupported(std::string reason);
  virtual std::shared_ptr<ConcreteVal> getInputValue(unsigned index,
                                                     const IR::Input &input);
  std::shared_ptr<ConcreteVal> getConstantValue(const IR::Value &i);
  ConcreteVal *getPoisonValue(const IR::Type &type);
  static const llvm::fltSemantics *getFloatSemantics(const IR::FloatType &type);

  bool isReturned() const {
    return !cur_block;
  }

  bool isUndefined() const {
    return UB_flag;
  }

  bool isUnsupported() const {
    return unsupported_flag;
  }

  bool isFinished() const {
    return isReturned() || isUndefined() || isUnsupported();
  }

  void printMemory(std::ostream &os) const {
    os << "init memory state\n";
    os << "[";
    for (auto &block : init_mem_blocks) {
      os << "{";
      block.print(os);
      os << "}";
    }
    os << "]\n";
    os << "final memory state\n";
    os << "[";
    for (auto &block : mem_blocks) {
      os << "{";
      block.print(os);
      os << "}";
    }
    os << "]";
  }

  ConcreteBlock &getBlock(uint64_t index) {
    assert(index < init_mem_blocks.size() && "block doesn't exist");
    return init_mem_blocks[index];
  }

  std::map<const IR::Value *, std::shared_ptr<ConcreteVal>> concrete_vals;
  const IR::BasicBlock *pred_block = nullptr;
  const IR::BasicBlock *cur_block = nullptr;
  unsigned pos_in_block = 0;
  bool UB_flag = false;
  bool unsupported_flag = false;
  std::string unsupported_reason;
  ConcreteVal *return_value = nullptr;
  std::vector<ConcreteBlock> init_mem_blocks;
  std::vector<ConcreteBlock> mem_blocks;
};

void interp(IR::Function &f);

} // namespace util
