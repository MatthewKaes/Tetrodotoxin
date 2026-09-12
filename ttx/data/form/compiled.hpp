// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/static/bytes.hpp"

#include "ttx/data/form/compiler.hpp"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Data::Form {

// A static Schema can pay its normalization cost during C++ translation.
// Compiled publishes the same byte format as the runtime compiler and lends
// it for the lifetime of its module, without retaining any preparation data.
//
// Exact array size is part of a C++ type. Measuring therefore prepares once,
// then construction prepares again and writes the final bytes. Keeping those
// two evaluations avoids a separate capacity census and fixed working graph.
// Measurement performs no serialization or final buffer allocation.
template <const Schema& source>
class Compiled {
 private:
  static consteval auto measure() -> Count {
    Compiler compiler;
    const auto status = compiler.compile(source);
    return status == Status::Success ? compiler.get_size() : 0;
  }

  static constexpr Count size = measure();
  static_assert(size != 0, "Invalid constant TTX Schema");

  struct Storage {
    Perimortem::Core::Static::Bytes<size> bytes;
    Representation representation;

    consteval Storage() : representation(bytes.get_data(), size) {
      // Measurement admitted this exact immutable source and established the
      // output extent. The repeated preparation follows the same algorithm,
      // so publication needs no second failure state or allocation policy.
      Compiler compiler;
      compiler.compile(source);
      compiler.write(Perimortem::Core::Access::Bytes(bytes.get_data(), size));
    }
  };

 public:
  static constexpr auto get_representation() -> const Representation& {
    static constexpr Storage storage;
    return storage.representation;
  }
};

}  // namespace Ttx::Data::Form
