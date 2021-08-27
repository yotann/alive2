#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include <jsoncons/byte_string.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/cbor/decode_cbor.hpp>
#include <jsoncons_ext/cbor/encode_cbor.hpp>

using jsoncons::ojson;
namespace IR { class Function; }

namespace util {

void interp(IR::Function &f, std::string test_input="None");
void interp_outline(IR::Function &f, ojson& result, std::string test_input="None");

}
