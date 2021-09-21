#include "util/worker.h"

#include "ir/type.h"
#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/config.h"
#include "util/errors.h"
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
using jsoncons::json_array_arg;
using jsoncons::json_object_arg;
using jsoncons::ojson;

static optional<smt::smt_initializer> smt_init;

static llvm::Function &getSoleDefinition(llvm::Module &m) {
  for (llvm::Function &f : m.functions())
    if (!f.isDeclaration())
      return f;
  std::cerr << "missing definition in module\n";
  exit(2);
}

namespace {
struct VerifyResults : public Errors {
  Transform t;
  ojson result;

  void checkErrs();
  bool addSolverError(const smt::Result &r) override;
  bool addSolverSatApprox(const std::string &approx) override;
  bool addSolverSat(const IR::State &src_state, const IR::State &tgt_state,
                    const smt::Result &r, const IR::Value *main_var,
                    const std::string &msg, bool check_each_var,
                    const std::string &post_msg) override;
};
} // end anonymous namespace

void VerifyResults::checkErrs() {
  // Be selective about when to overwrite "status" and "valid". They might
  // already be set to something more specific.
  if (*this) {
    if (isUnsound()) {
      result["status"] = "unsound";
      result["valid"] = false;
    } else if (!result.count("status")) {
      result["status"] = "unknown";
      result["valid"] = ojson(nullptr); // unknown
    }
  } else if (!result.count("status") && !result.count("valid")) {
    result["status"] = "sound";
    result["valid"] = true;
  }
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

static llvm::APInt bvToAPInt(const smt::expr &example) {
  llvm::APInt apint(example.bits(), 0);
  uint64_t u64;
  for (unsigned pos = 0; pos < example.bits(); pos += 64) {
    unsigned high = min(pos + 63, example.bits() - 1);
    assert(example.extract(high, pos).isUInt(u64));
    apint.insertBits(u64, pos, 64);
  }
  return apint;
}

static const llvm::fltSemantics *getFloatSemantics(const IR::FloatType &type) {
  switch (type.getFpType()) {
  case IR::FloatType::Half:
    return &llvm::APFloat::IEEEhalf();
  case IR::FloatType::Float:
    return &llvm::APFloat::IEEEsingle();
  case IR::FloatType::Double:
    return &llvm::APFloat::IEEEdouble();
  case IR::FloatType::Quad:
    return &llvm::APFloat::IEEEquad();
  default:
    return nullptr;
  }
}

static ojson modelValToJSON(const smt::Model &m, const IR::Type &type,
                            const IR::StateValue &val) {
  if (!val.isValid())
    return nullptr; // invalid expr
  // TODO: if Input, check for undef

  if (type.isAggregateType()) {
    const auto &agg = *type.getAsAggregateType();
    ojson result(json_array_arg);
    for (size_t i = 0; i < agg.numElementsConst(); ++i) {
      if (agg.isPadding(i))
        continue;
      result.push_back(modelValToJSON(m, agg.getChild(i), agg.extract(val, i)));
    }
    return result;
  }

  // Can't apply this to aggregates.
  if (auto v = m.eval(val.non_poison); (v.isFalse() || check_expr(!v).isSat()))
    return "poison";

  if (type.isIntType()) {
    smt::expr example = m.eval(val.value, true);
    int64_t i64;
    if (example.isInt(i64))
      return i64;
    return storeAPIntAsByteString(bvToAPInt(example));
  } else if (type.isFloatType()) {
    smt::expr example = m.eval(val.value, true).float2BV();
    llvm::APInt apint = bvToAPInt(example);
    if (auto semantics =
            getFloatSemantics(static_cast<const IR::FloatType &>(type))) {
      auto dbl = llvm::APFloat(*semantics, apint);
      bool loses_info;
      if (dbl.convert(llvm::APFloat::IEEEdouble(),
                      llvm::RoundingMode::NearestTiesToEven,
                      &loses_info) == llvm::APFloat::opOK) {
        return dbl.convertToDouble();
      }
    }
    return storeAPIntAsByteString(apint);
  } else if (type.isPtrType()) {
    return nullptr; // TODO
  } else if (&type == &IR::Type::voidTy) {
    return nullptr;
  }
  return nullptr; // unsupported
}

static ojson testInputToJSON(const IR::Function &f, const IR::State &state,
                             const smt::Model &m) {
  ojson result(json_object_arg);
  ojson &args = result["args"] = ojson(json_array_arg);
  for (const auto &input : f.getInputs()) {
    const auto &var = state.at(input);
    args.push_back(modelValToJSON(m, input.getType(), var.first));
  }
  return result;
}

bool VerifyResults::addSolverError(const smt::Result &r) {
  result["valid"] = nullptr; // unknown
  if (r.isInvalid()) {
    result["status"] = "invalid_expr";
    return true;
  } else if (r.isTimeout()) {
    result["status"] = "solver_timeout";
    return false;
  } else if (r.isError()) {
    result["status"] = "solver_error";
    add("SMT Error: " + r.getReason(), false);
    return false;
  } else if (r.isSkip()) {
    result["status"] = "solver_skip";
    return true;
  } else {
    UNREACHABLE();
  }
}

bool VerifyResults::addSolverSatApprox(const std::string &approx) {
  result["status"] = "unsound_with_approximations";
  result["valid"] = nullptr;
  add("Approximations done:\n" + approx, false);
  return false;
}

bool VerifyResults::addSolverSat(const IR::State &src_state,
                                 const IR::State &tgt_state,
                                 const smt::Result &r,
                                 const IR::Value *main_var,
                                 const std::string &msg, bool check_each_var,
                                 const std::string &post_msg) {
  add(string(msg), true);
  // tgt_state may be more defined than src_state, so use that.
  result["test_input"] = testInputToJSON(t.tgt, tgt_state, r.getModel());
  return false;
}

static VerifyResults verify(llvm::Function &f1, llvm::Function &f2,
                            llvm::TargetLibraryInfoWrapperPass &tli) {
  VerifyResults r;
  auto fn1 = llvm2alive(f1, tli.getTLI(f1));
  auto fn2 = llvm2alive(f2, tli.getTLI(f2));
  if (!fn1 || !fn2) {
    r.result["status"] = "could_not_translate";
    r.result["valid"] = ojson(nullptr); // unknown
    return r;
  }

  r.t.src = move(*fn1);
  r.t.tgt = move(*fn2);

  stringstream ss1, ss2;
  r.t.src.print(ss1);
  r.t.tgt.print(ss2);
  if (ss1.str() == ss2.str()) {
    r.result["status"] = "syntactic_eq";
    r.result["valid"] = true;
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
    r.result["status"] = "type_checker_failed";
    r.result["valid"] = false;
    return r;
  }
  assert(types.hasSingleTyping());

  verifier.verify(r);
  r.checkErrs();
  return r;
}

static ojson compareFunctions(llvm::Function &f1, llvm::Function &f2,
                              llvm::TargetLibraryInfoWrapperPass &tli) {
  auto r = verify(f1, f2, tli);
  if (r) {
    std::ostringstream sstr;
    sstr << r;
    sstr.flush();
    r.result["errs"] = sstr.str();
  }
  return r.result;
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
    auto semantics =
        getFloatSemantics(static_cast<const IR::FloatType &>(type));
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
  } else if (type.isVectorType() || type.isStructType()) {
    const auto &aggregate = static_cast<const IR::AggregateType &>(type);
    if (val.is_array()) {
      vector<shared_ptr<ConcreteVal>> elements;
      size_t val_i = 0;
      for (size_t i = 0; i < aggregate.numElementsConst(); ++i) {
        if (aggregate.isPadding(i)) {
          elements.push_back(make_shared<ConcreteValInt>(
              false, llvm::APInt(aggregate.getChild(i).bits(), 0)));
        } else {
          elements.push_back(shared_ptr<ConcreteVal>(
              loadConcreteVal(aggregate.getChild(i), val[val_i++])));
        }
        if (elements.back() == nullptr)
          return nullptr;
      }
      return new ConcreteValAggregate(false, move(elements));
    } else if (val == POISON) {
      // Make a vector of poison values, and then mark the vector as a whole as
      // poison.
      vector<shared_ptr<ConcreteVal>> elements;
      for (size_t i = 0; i < aggregate.numElementsConst(); ++i)
        elements.push_back(shared_ptr<ConcreteVal>(
            loadConcreteVal(aggregate.getChild(i), POISON)));
      return new ConcreteValAggregate(true, move(elements));
    }
  }
  // unsupported
  return nullptr;
}

static ojson storeConcreteVal(const IR::Type &type, const ConcreteVal *val) {
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
  } else if (auto val_vec = dynamic_cast<const ConcreteValAggregate *>(val)) {
    const auto &agg_type = static_cast<const IR::AggregateType &>(type);
    const auto &vec = val_vec->getVal();
    ojson::array tmp;
    tmp.reserve(agg_type.numElementsConst() - agg_type.numPaddingsConst());
    for (size_t i = 0; i < agg_type.numElementsConst(); ++i) {
      if (agg_type.isPadding(i))
        continue;
      tmp.emplace_back(storeConcreteVal(agg_type.getChild(i), vec[i].get()));
    }
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
    result["return_value"] =
        storeConcreteVal(fn->getType(), interpreter.return_value);
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
  config::disable_poison_input =
      options.get_with_default<bool>("disable_poison_input", false);
  config::disable_undef_input =
      options.get_with_default<bool>("disable_undef_input", false);

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
  if (func == "alive.tv_v2") {
    return evaluateAliveTV(job["args"][0], job["args"][1], job["args"][2]);
  } else if (func == "alive.interpret") {
    return evaluateAliveInterpret(job["args"][0], job["args"][1],
                                  job["args"][2]);
  } else {
    cerr << "unsupported func \"" << func << "\"\n";
    exit(1);
  }
}