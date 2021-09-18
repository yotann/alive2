#include "util/worker.h"

#include "ir/type.h"
#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/interp.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

#include <jsoncons/byte_string.hpp>
#include <jsoncons/json.hpp>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;
using jsoncons::byte_string_arg;
using jsoncons::json_object_arg;
using jsoncons::ojson;

static optional<smt::smt_initializer> smt_init;

static llvm::Function &getSoleDefinition(llvm::Module &m) {
  for (llvm::Function &f : m.functions())
    return f;
  std::cerr << "missing definition in module\n";
  _exit(2);
}

namespace {
struct VerifyResults {
  Transform t;
  Errors errs;
  enum {
    COULD_NOT_TRANSLATE,
    TYPE_CHECKER_FAILED,
    SYNTACTIC_EQ,
    CORRECT,
    UNSOUND,
    FAILED_TO_PROVE,
  } status;

  static VerifyResults error(string &&err) {
    VerifyResults r;
    r.status = COULD_NOT_TRANSLATE;
    r.errs.add(move(err), false);
    return r;
  }
};
} // end anonymous namespace

static VerifyResults verify(llvm::Function &f1, llvm::Function &f2,
                            llvm::TargetLibraryInfoWrapperPass &tli) {
  auto fn1 = llvm2alive(f1, tli.getTLI(f1));
  if (!fn1)
    return VerifyResults::error("Could not translate src to Alive IR\n");
  auto fn2 = llvm2alive(f2, tli.getTLI(f2));
  if (!fn2)
    return VerifyResults::error("Could not translate tgt to Alive IR\n");

  VerifyResults r;
  r.t.src = move(*fn1);
  r.t.tgt = move(*fn2);

  stringstream ss1, ss2;
  r.t.src.print(ss1);
  r.t.tgt.print(ss2);
  if (ss1.str() == ss2.str()) {
    r.status = VerifyResults::SYNTACTIC_EQ;
    return r;
  }

  if (smt_init)
    smt_init->reset();
  else
    smt_init.emplace();
  r.t.preprocess();
  TransformVerify verifier(r.t, false);

  auto types = verifier.getTypings();
  if (!types) {
    r.status = VerifyResults::TYPE_CHECKER_FAILED;
    return r;
  }
  assert(types.hasSingleTyping());

  r.errs = verifier.verify();
  if (r.errs) {
    r.status = r.errs.isUnsound() ? VerifyResults::UNSOUND
                                  : VerifyResults::FAILED_TO_PROVE;
  } else {
    r.status = VerifyResults::CORRECT;
  }
  return r;
}

static ojson compareFunctions(llvm::Function &f1, llvm::Function &f2,
                              llvm::TargetLibraryInfoWrapperPass &tli) {
  auto r = verify(f1, f2, tli);
  ojson result(json_object_arg);

  if (r.status == VerifyResults::UNSOUND ||
      r.status == VerifyResults::FAILED_TO_PROVE ||
      r.status == VerifyResults::COULD_NOT_TRANSLATE) {
    std::ostringstream sstr;
    sstr << r.errs;
    sstr.flush();
    result["errs"] = sstr.str();
  } else {
    assert(!r.errs);
  }

  if (r.status == VerifyResults::CORRECT ||
      r.status == VerifyResults::SYNTACTIC_EQ) {
    result["valid"] = true;
  } else if (r.status == VerifyResults::UNSOUND ||
             r.status == VerifyResults::TYPE_CHECKER_FAILED) {
    result["valid"] = false;
  } else {
    result["valid"] = ojson(nullptr); // unknown
  }

  switch (r.status) {
  case VerifyResults::CORRECT:
    result["outcome"] = "correct";
    break;
  case VerifyResults::COULD_NOT_TRANSLATE:
    result["outcome"] = "could_not_translate";
    break;
  case VerifyResults::FAILED_TO_PROVE:
    result["outcome"] = "failed_to_prove";
    break;
  case VerifyResults::SYNTACTIC_EQ:
    result["outcome"] = "syntactic_eq";
    break;
  case VerifyResults::TYPE_CHECKER_FAILED:
    result["outcome"] = "type_checker_failed";
    break;
  case VerifyResults::UNSOUND:
    result["outcome"] = "unsound";
    break;
  }

  return result;
}

static ConcreteVal *loadConcreteVal(const IR::Type &type, const ojson &val) {
  static const ojson POISON = "poison";
  if (&type == &IR::Type::voidTy) {
    return new ConcreteValVoid();
  } else if (type.isIntType()) {
    unsigned bits = type.bits();
    if (val.is_uint64()) {
      return new ConcreteValInt(false,
                                llvm::APInt(bits, val.as_integer<uint64_t>()));
    } else if (val.is_int64()) {
      return new ConcreteValInt(
          false, llvm::APInt(bits, val.as_integer<int64_t>(), true));
    } else if (val.is_byte_string()) {
      auto bsv = val.as_byte_string_view();
      llvm::APInt tmp(bits, 0);
      // big endian, like CBOR tag 2
      for (size_t i = 0; i < bsv.size(); ++i)
        tmp.insertBits(bsv[i], 8 * (bsv.size() - i - 1), 8);
      return new ConcreteValInt(false, move(tmp));
    } else if (val == POISON) {
      return new ConcreteValInt(true, llvm::APInt(bits, 0));
    }
  } else if (type.isFloatType()) {
    const llvm::fltSemantics *semantics;
    switch (static_cast<const IR::FloatType &>(type).getFpType()) {
    case IR::FloatType::Half:
      semantics = &llvm::APFloat::IEEEhalf();
      break;
    case IR::FloatType::Float:
      semantics = &llvm::APFloat::IEEEsingle();
      break;
    case IR::FloatType::Double:
      semantics = &llvm::APFloat::IEEEdouble();
      break;
    case IR::FloatType::Quad:
      semantics = &llvm::APFloat::IEEEquad();
      break;
    case IR::FloatType::Unknown:
      return nullptr;
    }
    if (val.is_double()) {
      llvm::APFloat tmp(val.as_double());
      bool loses_info;
      tmp.convert(*semantics, llvm::RoundingMode::NearestTiesToEven,
                  &loses_info);
      return new ConcreteValFloat(false, move(tmp));
    } else if (val == POISON) {
      return new ConcreteValFloat(true, llvm::APFloat::getZero(*semantics));
    }
    // TODO: support larger floats. Options are:
    // - Byte strings
    // - Bigfloats (CBOR tag 5), widely supported but can't represent NaN bits
    // - Extended bigfloats (CBOR tag 269), can represent NaN bits but only
    //   supported by one CBOR implementation
  } else if (type.isVectorType()) {
    const auto &aggregate = static_cast<const IR::AggregateType &>(type);
    if (val.is_array()) {
      vector<ConcreteVal *> elements;
      for (const ojson &subval : val.array_range()) {
        elements.push_back(loadConcreteVal(aggregate.getChild(0), subval));
        if (elements.back() == nullptr)
          return nullptr; // TODO: fix memory leak in elements
      }
      return new ConcreteValVect(false, move(elements));
    } else if (val == POISON) {
      // Make a vector of poison values, and then mark the vector as a whole as
      // poison.
      vector<ConcreteVal *> elements;
      for (size_t i = 0; i < aggregate.numElementsConst(); ++i)
        elements.push_back(loadConcreteVal(aggregate.getChild(0), POISON));
      return new ConcreteValVect(true, move(elements));
    }
  }
  // unsupported
  return nullptr;
}

static ojson storeAPIntAsByteString(const llvm::APInt &val) {
  // big endian
  vector<uint8_t> tmp;
  unsigned bytes = (val.getActiveBits() + 7) / 8;
  tmp.reserve(bytes);
  for (unsigned i = 0; i < bytes; ++i)
    tmp.push_back(val.extractBitsAsZExtValue(8, 8 * (bytes - i - 1)));
  return ojson(byte_string_arg, move(tmp));
}

static ojson storeConcreteVal(const ConcreteVal *val) {
  static const ojson POISON = "poison";
  static const ojson UNDEF = "undef";
  if (val->isPoison()) {
    return POISON;
  } else if (val->isUndef()) {
    return UNDEF;
  } else if (dynamic_cast<const ConcreteValVoid *>(val)) {
    return nullptr;
  } else if (auto val_int = dynamic_cast<const ConcreteValInt *>(val)) {
    auto i = val_int->getVal();
    if (i.isSignedIntN(64)) {
      return i.getSExtValue();
    } else if (i.isIntN(64)) {
      return i.getZExtValue();
    } else {
      return storeAPIntAsByteString(i);
    }
  } else if (auto val_float = dynamic_cast<const ConcreteValFloat *>(val)) {
    auto flt = val_float->getVal();
    auto dbl = flt;
    bool loses_info;
    if (dbl.convert(llvm::APFloat::IEEEdouble(),
                    llvm::RoundingMode::NearestTiesToEven,
                    &loses_info) == llvm::APFloat::opOK) {
      return dbl.convertToDouble();
    } else {
      return nullptr; // TODO
    }
  } else if (auto val_vec = dynamic_cast<const ConcreteValVect *>(val)) {
    const auto &vec = val_vec->getVal();
    ojson::array tmp;
    tmp.reserve(vec.size());
    for (auto item : vec)
      tmp.emplace_back(storeConcreteVal(item));
    return tmp;
  } else {
    return nullptr;
  }
}

namespace {
class WorkerInterpreter : public Interpreter {
public:
  WorkerInterpreter(const ojson &test_input);
  shared_ptr<ConcreteVal> getInputValue(unsigned index,
                                        const IR::Input &input) override;

  const ojson &test_input;
};
} // namespace

WorkerInterpreter::WorkerInterpreter(const ojson &test_input)
    : test_input(test_input) {}

shared_ptr<ConcreteVal>
WorkerInterpreter::getInputValue(unsigned index, const IR::Input &input) {
  return shared_ptr<ConcreteVal>(
      loadConcreteVal(input.getType(), test_input["args"][index]));
}

static ojson evaluateAliveInterpret(const ojson &options, const ojson &src,
                                    const ojson &test_input) {
  // TODO: it it better to use max_steps or use a timeout?
  uint64_t max_steps =
      options.get_with_default<uint64_t>("max_steps", uint64_t(1024));

  auto to_stringref = [](const ojson &node) {
    auto bytes = node.as_byte_string_view();
    return llvm::StringRef(reinterpret_cast<const char *>(bytes.data()),
                           bytes.size());
  };

  llvm::LLVMContext context;
  auto m_or_err = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(to_stringref(src), "src"), context);
  if (!m_or_err) {
    std::cerr << "could not parse bitcode file\n";
    _exit(1);
  }
  auto m = std::move(*m_or_err);

  auto &dl = m->getDataLayout();
  llvm::Triple target_triple(m->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass tli(target_triple);

  std::ostringstream out;
  llvm_util::initializer llvm_util_init(out, dl);
  // TODO: include contents of out.str() in the response.

  auto &f = getSoleDefinition(*m);
  auto fn = llvm2alive(f, tli.getTLI(f));
  if (!fn) {
    std::cerr << "could not translate src to Alive IR\n";
    _exit(1);
  }

  WorkerInterpreter interpreter(test_input);
  interpreter.start(*fn);
  interpreter.run(max_steps);
  ojson result(json_object_arg);
  if (interpreter.isUnsupported()) {
    result["status"] = "unsupported";
  } else if (interpreter.isUndefined()) {
    result["status"] = "done";
    result["undefined"] = true;
  } else if (interpreter.isReturned()) {
    result["status"] = "done";
    result["undefined"] = false;
    result["return_value"] = storeConcreteVal(interpreter.return_value);
  } else {
    result["status"] = "timeout";
  }
  return result;
}

static ojson evaluateAliveTV(const ojson &options, const ojson &src,
                             const ojson &tgt) {
  uint64_t smt_timeout =
      options.get_with_default<uint64_t>("smt_timeout", 10000);
  uint64_t smt_max_mem =
      options.get_with_default<uint64_t>("smt_max_mem", 1024);
  uint64_t smt_random_seed =
      options.get_with_default<uint64_t>("smt_random_seed", uint64_t(0));

  auto to_stringref = [](const ojson &node) {
    auto bytes = node.as_byte_string_view();
    return llvm::StringRef(reinterpret_cast<const char *>(bytes.data()),
                           bytes.size());
  };

  smt::set_query_timeout(to_string(smt_timeout));
  smt::set_memory_limit(smt_max_mem * 1024 * 1024);
  smt::set_random_seed(to_string(smt_random_seed));

  llvm::LLVMContext context;
  auto m1_or_err = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(to_stringref(src), "src"), context);
  auto m2_or_err = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(to_stringref(tgt), "tgt"), context);
  if (!m1_or_err || !m2_or_err) {
    std::cerr << "could not parse bitcode files\n";
    _exit(1);
  }
  auto m1 = std::move(*m1_or_err), m2 = std::move(*m2_or_err);

  auto &dl = m1->getDataLayout();
  llvm::Triple target_triple(m1->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass tli(target_triple);

  std::ostringstream out;
  llvm_util::initializer llvm_util_init(out, dl);
  // TODO: include contents of out.str() in the response.

  if (m1->getTargetTriple() != m2->getTargetTriple()) {
    std::cerr << "module target triples do not match";
    abort();
  }

  auto &f1 = getSoleDefinition(*m1);
  auto &f2 = getSoleDefinition(*m2);
  auto result = compareFunctions(f1, f2, tli);
  return result;
}

ojson util::evaluateAliveFunc(const ojson &job) {
  string func = job["func"].as<string>();
  if (func == "alive.tv") {
    return evaluateAliveTV(job["args"][0], job["args"][1], job["args"][2]);
  } else if (func == "alive.interpret") {
    return evaluateAliveInterpret(job["args"][0], job["args"][1],
                                  job["args"][2]);
  } else {
    cerr << "unsupported func \"" << func << "\"\n";
    exit(1);
  }
}
