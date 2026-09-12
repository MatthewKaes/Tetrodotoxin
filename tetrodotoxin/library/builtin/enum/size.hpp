// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/types/unsigned.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/model/documentations/comment.hpp"

namespace Tetrodotoxin::Library::Builtin::Enum {

// Size is the Static immutable case count of one Enumeration.
class Size : public Language::Model::Memory {
 public:
  static constexpr Perimortem::Core::View::Bytes name = "size"_view;

  TTX_CONTRACT(Size, Language::Model::Memory);

  static auto create(
      Perimortem::Memory::Allocator::Arena& domain,
      const Language::Model::Types::Unsigned& type,
      Count count) -> Size&;

  TTX_NAME(name);
  TTX_DOCUMENTATION(documentation);

  constexpr auto get_type() const
      -> const Language::Model::Types::Unsigned& override {
    return type;
  }

  constexpr auto resolve() const -> const Ttx::Concept::Abstract& override {
    return *this;
  }

  constexpr auto get_constant() const
      -> Perimortem::Core::Option<Language::Model::Pack&> override {
    return constant;
  }

 private:
  constexpr Size(
      const Language::Model::Types::Unsigned& type,
      Language::Model::Pack& constant)
      : type(type), constant(constant) {}

  const Language::Model::Types::Unsigned& type;
  Language::Model::Pack& constant;

  static constexpr Ttx::Model::Documentations::Comment documentation{
    "Provides the compile time number of cases in this Enumeration."_view,
  };
};

}  // namespace Tetrodotoxin::Library::Builtin::Enum
