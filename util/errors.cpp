// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/errors.h"

#include <set>
#include <sstream>
#include <string>

#include "ir/constant.h"
#include "ir/globals.h"

using namespace IR;
using namespace smt;
using namespace std;

namespace util {

Errors::~Errors() {}

void Errors::add(const char *str, bool is_unsound) {
  add(string(str), is_unsound);
}

void Errors::add(string &&str, bool is_unsound) {
  if (is_unsound)
    errs.clear();
  errs.emplace(move(str), is_unsound);
}

void Errors::add(AliveException &&e) {
  add(move(e.msg), e.is_unsound);
}

bool Errors::isUnsound() const {
  for (auto &[msg, unsound] : errs) {
    if (unsound)
      return true;
  }
  return false;
}

ostream& operator<<(ostream &os, const Errors &errs) {
  for (auto &[msg, unsound] : errs.errs) {
    os << "ERROR: " << msg << '\n';
  }
  return os;
}

static bool is_arbitrary(const expr &e) {
  if (e.isConst())
    return false;
  return check_expr(expr::mkForAll(e.vars(), expr::mkVar("#someval", e) != e))
      .isUnsat();
}

static void print_single_varval(ostream &os, const State &st, const Model &m,
                                const Value *var, const Type &type,
                                const StateValue &val, unsigned child) {
  if (!val.isValid()) {
    os << "(invalid expr)";
    return;
  }

  // Best effort detection of poison if model is partial
  if (auto v = m.eval(val.non_poison);
      (v.isFalse() || check_expr(!v).isSat())) {
    os << "poison";
    return;
  }

  if (auto *in = dynamic_cast<const Input *>(var)) {
    auto var = in->getUndefVar(type, child);
    if (var.isValid() && m.eval(var, false).isAllOnes()) {
      os << "undef";
      return;
    }
  }

  // TODO: detect undef bits (total or partial) with an SMT query

  expr partial = m.eval(val.value);
  if (is_arbitrary(partial)) {
    os << "any";
    return;
  }

  type.printVal(os, st, m.eval(val.value, true));

  // undef variables may not have a model since each read uses a copy
  // TODO: add intervals of possible values for ints at least?
  if (!partial.isConst()) {
    // some functions / vars may not have an interpretation because it's not
    // needed, not because it's undef
    for (auto &var : partial.vars()) {
      if (isUndef(var)) {
        os << "\t[based on undef value]";
        break;
      }
    }
  }
}

void Errors::print_varval(ostream &os, const State &st, const Model &m,
                          const Value *var, const Type &type,
                          const StateValue &val, unsigned child) {
  if (!type.isAggregateType()) {
    print_single_varval(os, st, m, var, type, val, child);
    return;
  }

  os << (type.isStructType() ? "{ " : "< ");
  auto agg = type.getAsAggregateType();
  for (unsigned i = 0, e = agg->numElementsConst(); i < e; ++i) {
    if (i != 0)
      os << ", ";
    print_varval(os, st, m, var, agg->getChild(i), agg->extract(val, i),
                 child + i);
  }
  os << (type.isStructType() ? " }" : " >");
}

bool Errors::addSolverError(const Result &r) {
  if (r.isInvalid()) {
    add("Invalid expr", false);
    return true;
  }

  if (r.isTimeout()) {
    add("Timeout", false);
    return false;
  }

  if (r.isError()) {
    add("SMT Error: " + r.getReason(), false);
    return false;
  }

  if (r.isSkip()) {
    add("Skip", false);
    return true;
  }

  UNREACHABLE();
}

bool Errors::addSolverSatApprox(const std::string &approx) {
  stringstream s;
  s << "Couldn't prove the correctness of the transformation\n"
       "Alive2 approximated the semantics of the programs and therefore we\n"
       "cannot conclude whether the bug found is valid or not.\n\n"
       "Approximations done:\n";
  s << approx;
  add(s.str(), false);
  return false;
}

bool Errors::addSolverSat(const IR::State &src_state,
                          const IR::State &tgt_state, const smt::Result &r,
                          const IR::Value *main_var, const std::string &msg,
                          bool check_each_var, const std::string &post_msg) {
  stringstream s;
  auto &m = r.getModel();

  s << msg;
  if (main_var)
    s << " for " << *main_var;
  s << "\n\nExample:\n";

  for (auto &[var, val] : src_state.getValues()) {
    if (!dynamic_cast<const Input *>(var) &&
        !dynamic_cast<const ConstantInput *>(var))
      continue;
    s << *var << " = ";
    print_varval(s, src_state, m, var, var->getType(), val.first);
    s << '\n';
  }

  set<string> seen_vars;
  for (auto st : {&src_state, &tgt_state}) {
    if (!check_each_var) {
      if (st->isSource()) {
        s << "\nSource:\n";
      } else {
        s << "\nTarget:\n";
      }
    }

    for (auto &[var, val] : st->getValues()) {
      auto &name = var->getName();
      if (main_var && name == main_var->getName())
        break;

      if (name[0] != '%' || dynamic_cast<const Input *>(var) ||
          (check_each_var && !seen_vars.insert(name).second))
        continue;

      s << *var << " = ";
      print_varval(s, const_cast<State &>(*st), m, var, var->getType(),
                   val.first);
      s << '\n';
    }

    st->getMemory().print(s, m);
  }

  s << post_msg;
  add(s.str(), true);
  return false;
}

} // namespace util
