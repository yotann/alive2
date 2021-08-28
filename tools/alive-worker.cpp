// Copyright (c) 2021-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

// This program connects to a memodb-server server, gets alive.tv and
// alive.interpret jobs, evaluates them, and sends the results back to the
// server. You need a separate program to submit jobs to the server, or else
// this program will have nothing to do.
//
// When a crash or timeout happens, it's important to send an error message
// back to the server, so it knows not to retry the job. Alive2 doesn't have a
// good way to detect or recover from crashes, so we set up some extra threads
// and signal handlers to send an error result to the server when a crash or
// timeout happens. We still can't recover from the crash, and we still can't
// interrupt the main Alive2 thread if there's a timeout, so this program will
// exit after sending the error result.
//
// Here's a suggested way to run this program:
//
//   yes | parallel -n0 alive-worker http://127.0.0.1:1234
//
// This command will run one alive-worker process per core. Whenever a process
// exits, it will start a new one to replace it.

#include "llvm_util/llvm2alive.h"
#include "smt/smt.h"
#include "tools/transform.h"
#include "util/version.h"
#include "util/interp.h"
#include "util/concreteval.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Error.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

// The server may wait 60 seconds to respond if there are no jobs available yet.
#define CPPHTTPLIB_READ_TIMEOUT_SECOND 120

#include <httplib.h>
#include <jsoncons/byte_string.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/cbor/decode_cbor.hpp>
#include <jsoncons_ext/cbor/encode_cbor.hpp>

using namespace tools;
using namespace util;
using namespace std;
using namespace llvm_util;
using jsoncons::byte_string_arg;
using jsoncons::decode_base64url;
using jsoncons::encode_base64url;
using jsoncons::json_array_arg;
using jsoncons::json_object_arg;
using jsoncons::ojson;
using jsoncons::cbor::decode_cbor;
using jsoncons::cbor::encode_cbor;

static llvm::cl::OptionCategory alive_cmdargs("Alive2 worker options");
static llvm::cl::opt<string> opt_url(llvm::cl::Positional, llvm::cl::Required,
                                     llvm::cl::desc("<memodb server URL>"),
                                     llvm::cl::value_desc("url"),
                                     llvm::cl::cat(alive_cmdargs));

static llvm::ExitOnError exit_on_error;

static optional<smt::smt_initializer> smt_init;

// Used by signal handler to communicate with crash handler thread.
static sem_t crash_sem;
static volatile sig_atomic_t crash_handler_done = 0;

static std::mutex timeout_mutex;
static std::condition_variable timeout_cv;
static size_t timeout_millis = 0;
static size_t timeout_index = 0;
static bool timeout_cancelled = false;

static std::mutex result_mutex;
// May be empty if no result is in progress.
static std::string result_uri;
static std::optional<httplib::Client> session;

static const httplib::Headers cbor_headers{{"Accept", "application/cbor"}};

static ojson textCIDToBinary(const llvm::StringRef &text) {
  if (!text.startswith("u")) {
    llvm::errs() << "unsupported CID: " << text << "\n";
    std::exit(1);
  }
  std::vector<uint8_t> bytes{0x00};
  auto result = decode_base64url(text.begin() + 1, text.end(), bytes);
  if (result.ec != jsoncons::conv_errc::success) {
    llvm::errs() << "invalid CID: " << text << "\n";
    std::exit(1);
  }
  return ojson(byte_string_arg, bytes, 42);
}

static std::string binaryCIDToText(const ojson &cid) {
  std::string text = "u";
  auto bytes = cid.as_byte_string_view();
  if (bytes.size() == 0) {
    llvm::errs() << "invalid CID\n";
    std::exit(1);
  }
  encode_base64url(bytes.begin() + 1, bytes.end(), text);
  return text;
}

// Check http response and exit if there is no response or we receive an error response
static void checkResult(const httplib::Result &result) {
  if (!result) {
    std::cerr << "HTTP connection error: " << result.error() << '\n';
    std::exit(1);
  }
  if (result->status < 200 || result->status > 299) {
    std::cerr << "HTTP response error: " << result->body << "\n";
    std::exit(1);
  }
};

static ojson getNodeFromCID(const ojson &cid) {
  std::string uri = "/cid/" + binaryCIDToText(cid);
  auto result = session->Get(uri.c_str(), cbor_headers);
  checkResult(result);
  if (result->get_header_value("Content-Type") != "application/cbor") {
    std::cerr << "unexpected Content-Type!\n";
    std::exit(1);
  }
  return decode_cbor<ojson>(result->body);
};

static ojson putNode(const ojson &node) {
  std::string buffer;
  encode_cbor(node, buffer);
  auto result = session->Post("/cid", cbor_headers, buffer, "application/cbor");
  checkResult(result);
  string cid_str = result->get_header_value("Location");
  if (!llvm::StringRef(cid_str).startswith("/cid/")) {
    llvm::errs() << "invalid CID URI!\n";
    std::exit(1);
  }
  return textCIDToBinary(llvm::StringRef(cid_str).drop_front(5));
}

static void sendResult(const ojson &node) {
  std::lock_guard lock(result_mutex);
  if (result_uri.empty())
    return;
  auto cid = putNode(node);
  std::string buffer;
  encode_cbor(cid, buffer);
  auto result = session->Put(result_uri.c_str(), cbor_headers, buffer,
                             "application/cbor");
  checkResult(result);
  result_uri.clear();
}

// Must be signal-safe.
static void signalHandler(void *) {
  // Resume the signal handler thread.
  sem_post(&crash_sem);

  // Wait up to 10 seconds for the thread to send a result.
  for (int i = 0; i < 10; ++i) {
    if (crash_handler_done)
      break;
    sleep(1);
  }

  // Return and let LLVM's normal signal handlers run.
}

static void crashHandlerThread() {
  // Don't handle any signals in this thread.
  sigset_t sig_set;
  sigfillset(&sig_set);
  pthread_sigmask(SIG_BLOCK, &sig_set, nullptr);

  // Wait for signalHandler() to tell us a signal has been raised.
  while (sem_wait(&crash_sem))
    ;

  std::cerr << "crashed\n";
  sendResult(ojson(json_object_arg, {{"status", "crashed"}}));

  // Tell signalHandler() we're done.
  crash_handler_done = 1;
}

static void timeoutThread() {
  std::unique_lock<std::mutex> lock(timeout_mutex);
  while (true) {
    auto expected_millis = timeout_millis;
    auto expected_index = timeout_index;
    if (!expected_millis) {
      timeout_cv.wait(lock);
    } else {
      // If timeout_millis or timeout_index changes while we're waiting, the
      // job completed before the timeout. But if they don't change for
      // duration, the job hasn't completed yet.
      auto duration = std::chrono::milliseconds(expected_millis);
      auto predicate = [&] {
        return timeout_millis != expected_millis ||
               timeout_index != expected_index || timeout_cancelled;
      };
      if (!timeout_cv.wait_for(lock, duration, predicate))
        break; // Timeout occurred! (or cancelled)
    }
    if (timeout_cancelled)
      return;
  }

  sendResult(ojson(json_object_arg, {{"status", "timeout"}}));

  // We can't call exit() because that would destroy the global SMT context
  // while another thread might be using it.
  _exit(2);
}

static void exitHandler() {
  // We need to cancel the timeout mutex so it stops waiting on timeout_cv. The
  // timeout_cv variable will be destroyed when the program exits, and it's
  // illegal to destroy a condition variable that has threads waiting on it.
  std::lock_guard lock(timeout_mutex);
  timeout_cancelled = true;
  timeout_cv.notify_all();
}

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

  smt_init->reset();
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

static ojson interpFunction(llvm::Function &f, llvm::TargetLibraryInfoWrapperPass &tli,
                  const ojson &test_input) {
  auto fn = llvm2alive(f, tli.getTLI(f));
  string input_str = test_input.get_with_default<string>("args", "None");

  if (!fn) {
    cerr << "ERROR: Could not translate '" << f.getName().str()
         << "' to Alive IR\n";
    _exit(1);
  }
  ojson result(json_object_arg);
  // TODO remove this after debugging
  fn->print(cout << "\n----------------------------------------\n");
  // TODO interp should return result values to correctly update successCount 
  interp_outline(*fn, result, input_str);
  result["res"] = "foo bar baz";
  return result;
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

static ojson evaluateAliveInterpret(const ojson &options, const ojson &src,
                                    const ojson &test_input) {
  uint64_t timeout = options.get_with_default<uint64_t>("timeout", 60000);
  
  auto to_stringref = [](const ojson &node) {
    auto bytes = node.as_byte_string_view();
    return llvm::StringRef(reinterpret_cast<const char *>(bytes.data()),
                           bytes.size());
  };
  llvm::LLVMContext context;
  auto m1_or_err = llvm::parseBitcodeFile(
      llvm::MemoryBufferRef(to_stringref(src), "src"), context);
  if (!m1_or_err) {
    std::cerr << "could not parse bitcode files";
    _exit(1);
  }
  auto m1 = std::move(*m1_or_err);

  auto &dl = m1->getDataLayout();
  llvm::Triple target_triple(m1->getTargetTriple());
  llvm::TargetLibraryInfoWrapperPass tli(target_triple);

  std::ostringstream out;
  llvm_util::initializer llvm_util_init(out, dl);
  // TODO: include contents of out.str() in the response.

  auto &f1 = getSoleDefinition(*m1);

  std::unique_lock lock(timeout_mutex);
  timeout_millis = timeout;
  timeout_index++;
  lock.unlock();
  timeout_cv.notify_one();

  auto result = interpFunction(f1, tli, test_input);

  lock.lock();
  timeout_millis = 0;
  lock.unlock();
  timeout_cv.notify_one();

  return result;
}

static ojson evaluateAliveTV(const ojson &options, const ojson &src,
                             const ojson &tgt) {
  uint64_t smt_timeout =
      options.get_with_default<uint64_t>("smt_timeout", 10000);
  uint64_t timeout = options.get_with_default<uint64_t>("timeout", 60000);
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
    std::cerr << "could not parse bitcode files";
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

  std::unique_lock lock(timeout_mutex);
  timeout_millis = timeout;
  timeout_index++;
  lock.unlock();
  timeout_cv.notify_one();

  auto result = compareFunctions(f1, f2, tli);

  lock.lock();
  timeout_millis = 0;
  lock.unlock();
  timeout_cv.notify_one();

  return result;
}

int main(int argc, char **argv) {
  // Register our signal handler before LLVM's stack trace printer, so our
  // handler will run first.
  sem_init(&crash_sem, 0, 0);
  llvm::sys::AddSignalHandler(signalHandler, nullptr);

  // Start our threads before calling anything else from LLVM, in case LLVM's
  // functions crash.
  atexit(exitHandler);
  std::thread(crashHandlerThread).detach();
  std::thread(timeoutThread).detach();

  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::EnableDebugBuffering = true;
  llvm::llvm_shutdown_obj llvm_shutdown; // Call llvm_shutdown() on exit.
  smt_init.emplace();

  std::string Usage =
      R"EOF(Alive2 stand-alone distributed worker:
version )EOF";
  Usage += alive_version;
  Usage += R"EOF(
see alive-worker --version  for LLVM version info,

This program connects to a memodb-server server and evaluates Alive2-related
calls that are submitted to the server by other programs.
)EOF";

  llvm::cl::HideUnrelatedOptions(alive_cmdargs);
  llvm::cl::ParseCommandLineOptions(argc, argv, Usage);

  std::string server_url = opt_url;
  // httplib doesn't like the ending slash in http://127.0.0.1:1234/
  if (!server_url.empty() && server_url.back() == '/')
    server_url.pop_back();
  session.emplace(server_url);

  // Upload the list of funcs we can evaluate using POST /cid.
  ojson worker_info(
      json_object_arg,
      {
          {"funcs", ojson(json_array_arg, {"alive.tv", "alive.interpret"})},
      });
  // std::cout << "pretty print" << jsoncons::pretty_print(worker_info) << '\n';
  auto worker_info_cid = putNode(worker_info);

  while (true) {
    std::string buffer;
    encode_cbor(worker_info_cid, buffer);
    auto result =
        session->Post("/worker", cbor_headers, buffer, "application/cbor");
    checkResult(result);
    if (result->get_header_value("Content-Type") != "application/cbor") {
      std::cerr << "unexpected Content-Type!\n";
      return 1;
    }
    ojson job = decode_cbor<ojson>(result->body);
    if (job.is_null()) {
      // No jobs available, try again.
      sleep(1); // TODO: exponential backoff
      continue;
    }

    string func = job["func"].as<string>();
    std::string call_uri = "/call/" + func + "/";
    for (const ojson &arg : job["args"].array_range())
      call_uri += binaryCIDToText(arg) + ",";
    call_uri.pop_back(); // remove last comma
    llvm::errs() << "evaluating " << call_uri << "\n";

    std::unique_lock lock(result_mutex);
    result_uri = call_uri;
    lock.unlock();
    if (func == "alive.tv") {
      ojson options = getNodeFromCID(job["args"][0]);
      ojson src = getNodeFromCID(job["args"][1]);
      ojson tgt = getNodeFromCID(job["args"][2]);
      ojson result = evaluateAliveTV(options, src, tgt);
      sendResult(result);
    } else if (func == "alive.interpret") {
      ojson options = getNodeFromCID(job["args"][0]);
      ojson src = getNodeFromCID(job["args"][1]);
      ojson test_input = getNodeFromCID(job["args"][2]);
      ojson result = evaluateAliveInterpret(options, src, test_input);
      sendResult(result);
    } else {
      std::cerr << "unsupported func \"" << func << "\"\n";
      return 1;
    }
  }

  smt_init.reset();

  return 0;
}
