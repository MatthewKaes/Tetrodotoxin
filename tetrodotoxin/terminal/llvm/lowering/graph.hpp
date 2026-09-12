// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/monograph.hpp"
#include "tetrodotoxin/terminal/llvm/module/program.hpp"

namespace Tetrodotoxin::Terminal::Llvm::Lowering {

// Graph is the LLVM entry into one completed Library Monograph. It walks the
// real semantic identities through reservation, target completion, and source
// ordered emission without attaching physical state to the Dialect.
class Graph {
 public:
  static auto lower(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Monograph& monograph,
      Perimortem::Core::View::Vector<Ttx::Concept::Reference<
          const Tetrodotoxin::Library::Language::Model::Callable>> excluded =
          {}) -> Bool;

  static auto prepare(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Callable& callable) -> Bool;

  static auto prepare(
      Module::Program& program,
      const Ttx::Model::Addressable& addressable) -> Bool;
};

}  // namespace Tetrodotoxin::Terminal::Llvm::Lowering
