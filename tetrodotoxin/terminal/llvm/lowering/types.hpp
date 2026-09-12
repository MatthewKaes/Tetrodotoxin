// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "tetrodotoxin/terminal/llvm/module/program.hpp"

namespace Tetrodotoxin::Terminal::Llvm::Lowering {

// Types owns target carrier preparation for exact Library identities. The
// semantic graph supplies category, width, Layout, and declaration facts while
// this Terminal owns every physical representation and completion record.
class Types {
 public:
  static auto prepare(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Type& type) -> Bool;

  static auto prepare(
      Module::Program& program,
      const Ttx::Model::Addressable& addressable) -> Bool;

  static auto prepare(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Callable& callable) -> Bool;

  static auto reserve(
      Module::Program& program,
      const Ttx::Model::Addressable& addressable) -> Bool;

  static auto complete(
      Module::Program& program,
      const Ttx::Model::Addressable& addressable) -> Bool;

  static auto reserve(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Callable& callable) -> Bool;

  static auto complete(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Callable& callable) -> Bool;

  static auto reserve_declaration(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Type& type) -> Bool;

  static auto complete_declaration(
      Module::Program& program,
      const Tetrodotoxin::Library::Language::Model::Type& type) -> Bool;
};

}  // namespace Tetrodotoxin::Terminal::Llvm::Lowering
