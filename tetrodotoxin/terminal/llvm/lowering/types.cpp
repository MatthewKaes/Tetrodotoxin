// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/terminal/llvm/lowering/types.hpp"

#include "tetrodotoxin/library/language/model/types/signed.hpp"
#include "tetrodotoxin/library/language/model/types/value.hpp"
#include "tetrodotoxin/library/language/types/access.hpp"
#include "tetrodotoxin/library/language/types/composite.hpp"
#include "tetrodotoxin/library/language/types/enumeration.hpp"
#include "tetrodotoxin/library/language/types/fixed.hpp"
#include "tetrodotoxin/library/language/types/implementation.hpp"
#include "tetrodotoxin/library/language/types/interface.hpp"
#include "tetrodotoxin/library/language/types/object.hpp"
#include "tetrodotoxin/library/language/types/object_storage.hpp"
#include "tetrodotoxin/library/language/types/option.hpp"
#include "tetrodotoxin/library/language/types/range.hpp"
#include "tetrodotoxin/library/language/types/result.hpp"
#include "tetrodotoxin/library/language/types/source.hpp"
#include "tetrodotoxin/library/language/types/structure.hpp"
#include "tetrodotoxin/library/language/types/view.hpp"
#include "tetrodotoxin/terminal/abi/representation/type.hpp"
#include "tetrodotoxin/terminal/llvm/module/carriers.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin::Terminal;
using namespace Tetrodotoxin::Library::Language;

static auto reserve_value(
    Llvm::Module::Program& program,
    const Ttx::Concept::Abstract& answer) -> Bool;

static auto complete_value(
    Llvm::Module::Program& program,
    const Ttx::Concept::Abstract& answer) -> Bool;

static auto reserve_layout(
    Llvm::Module::Program& program,
    const Ttx::Concept::Layout& layout) -> Bool;

static auto complete_layout(
    Llvm::Module::Program& program,
    const Ttx::Concept::Layout& layout) -> Bool;

static auto reserve_value(
    Llvm::Module::Program& program,
    const Ttx::Concept::Abstract& answer) -> Bool {
  auto selected = answer.select<Model::Type>();
  if (!selected) {
    selected = answer.resolve().select<Model::Type>();
  }
  BAIL_IF(!selected);
  const Model::Type& type = *selected;
  auto kind = Tetrodotoxin::Terminal::Abi::Representation::Type::get_kind(type);
  BAIL_IF(!kind);
  auto reserved = program.get_carriers().reserve(program, type, *kind);
  BAIL_IF(!reserved);
  if (!*reserved) {
    return True;
  }

  auto enumeration = type.select<Types::Enumeration>();
  if (enumeration) {
    auto storage = enumeration->get_storage_type();
    return storage && reserve_value(program, *storage);
  }
  auto fixed = type.select<Types::Fixed>();
  if (fixed) {
    return reserve_value(program, fixed->get_element_type());
  }
  auto option = type.select<Types::Option>();
  if (option) {
    return reserve_value(program, option->get_element_type()) &&
           reserve_value(program, option->get_flag_type());
  }
  auto result = type.select<Types::Result>();
  if (result) {
    return reserve_value(program, result->get_value_type()) &&
           reserve_value(program, result->get_error_type()) &&
           reserve_value(program, result->get_flag_type());
  }
  auto range = type.select<Types::Range>();
  if (range) {
    return reserve_value(program, range->get_element_type());
  }
  auto view = type.select<Types::View>();
  if (view) {
    return reserve_value(program, view->get_element_type());
  }
  auto access = type.select<Types::Access>();
  if (access) {
    return reserve_value(program, access->get_element_type());
  }
  auto implementation = type.select<Types::Implementation>();
  if (implementation) {
    auto interface =
        implementation->get_requirement().resolve().select<Types::Interface>();
    if (interface) {
      for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
           interface->get_addressables(
               Tetrodotoxin::Language::Visibility::Public)) {
        auto addressable = candidate.get().select<Model::Memory>();
        if (!addressable || !addressable->contributes_to_instance_layout() ||
            !reserve_value(program, addressable->get_type())) {
          return False;
        }
      }
    }
  }
  auto storage = type.select<Types::ObjectStorage>();
  if (storage) {
    return reserve_value(program, storage->get_element_type());
  }
  auto composite = type.select<Types::Composite>();
  if (composite) {
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         composite->get_addressables()) {
      auto addressable = candidate.get().select<Model::Memory>();
      if (addressable && addressable->contributes_to_instance_layout() &&
          !reserve_value(program, addressable->get_type())) {
        return False;
      }
    }
  }
  return True;
}

static auto complete_value(
    Llvm::Module::Program& program,
    const Ttx::Concept::Abstract& answer) -> Bool {
  auto selected = answer.select<Model::Type>();
  if (!selected) {
    selected = answer.resolve().select<Model::Type>();
  }
  BAIL_IF(!selected);
  const Model::Type& type = *selected;
  auto kind = Tetrodotoxin::Terminal::Abi::Representation::Type::get_kind(type);
  BAIL_IF(!kind);
  auto began = program.get_carriers().begin_completion(program, type);
  BAIL_IF(!began);
  if (!*began) {
    return True;
  }

  auto enumeration = type.select<Types::Enumeration>();
  if (enumeration) {
    auto storage = enumeration->get_storage_type();
    BAIL_IF(!storage || !complete_value(program, *storage));
  }
  auto fixed = type.select<Types::Fixed>();
  if (fixed && !complete_value(program, fixed->get_element_type())) {
    return False;
  }
  auto option = type.select<Types::Option>();
  if (option && (!complete_value(program, option->get_element_type()) ||
                 !complete_value(program, option->get_flag_type()))) {
    return False;
  }
  auto result = type.select<Types::Result>();
  if (result && (!complete_value(program, result->get_value_type()) ||
                 !complete_value(program, result->get_error_type()) ||
                 !complete_value(program, result->get_flag_type()))) {
    return False;
  }
  auto range = type.select<Types::Range>();
  if (range && !complete_value(program, range->get_element_type())) {
    return False;
  }
  auto view = type.select<Types::View>();
  if (view && !complete_value(program, view->get_element_type())) {
    return False;
  }
  auto access = type.select<Types::Access>();
  if (access && !complete_value(program, access->get_element_type())) {
    return False;
  }
  auto implementation = type.select<Types::Implementation>();
  if (implementation) {
    auto interface =
        implementation->get_requirement().resolve().select<Types::Interface>();
    if (interface) {
      for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
           interface->get_addressables(
               Tetrodotoxin::Language::Visibility::Public)) {
        auto addressable = candidate.get().select<Model::Memory>();
        if (!addressable || !addressable->contributes_to_instance_layout() ||
            !complete_value(program, addressable->get_type())) {
          return False;
        }
      }
    }
  }
  auto storage = type.select<Types::ObjectStorage>();
  if (storage && !complete_value(program, storage->get_element_type())) {
    return False;
  }
  auto composite = type.select<Types::Composite>();
  if (composite) {
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         composite->get_addressables()) {
      auto addressable = candidate.get().select<Model::Memory>();
      if (addressable && addressable->contributes_to_instance_layout() &&
          !complete_value(program, addressable->get_type())) {
        return False;
      }
    }
  }

  return program.get_carriers().complete(program, type, *kind) &&
         program.get_debug().type(type, type.get_declaration_anchor());
}

static auto reserve_layout(
    Llvm::Module::Program& program,
    const Ttx::Concept::Layout& layout) -> Bool {
  for (Count index = 0; index < layout.get_size(); index++) {
    auto entry = layout.get_abstract(index);
    BAIL_IF(!entry);
    auto addressable = entry->select<Ttx::Model::Addressable>();
    const Ttx::Concept::Abstract& answer =
        addressable ? addressable->get_type() : *entry;
    BAIL_IF(!reserve_value(program, answer));
  }
  return True;
}

static auto complete_layout(
    Llvm::Module::Program& program,
    const Ttx::Concept::Layout& layout) -> Bool {
  for (Count index = 0; index < layout.get_size(); index++) {
    auto entry = layout.get_abstract(index);
    BAIL_IF(!entry);
    auto addressable = entry->select<Ttx::Model::Addressable>();
    const Ttx::Concept::Abstract& answer =
        addressable ? addressable->get_type() : *entry;
    BAIL_IF(!complete_value(program, answer));
  }
  return True;
}

auto Llvm::Lowering::Types::prepare(
    Llvm::Module::Program& program,
    const Model::Type& type) -> Bool {
  return reserve_value(program, type) && complete_value(program, type);
}

auto Llvm::Lowering::Types::prepare(
    Llvm::Module::Program& program,
    const Ttx::Model::Addressable& addressable) -> Bool {
  return reserve(program, addressable) && complete(program, addressable);
}

auto Llvm::Lowering::Types::reserve(
    Llvm::Module::Program& program,
    const Ttx::Model::Addressable& addressable) -> Bool {
  return reserve_value(program, addressable.get_type());
}

auto Llvm::Lowering::Types::complete(
    Llvm::Module::Program& program,
    const Ttx::Model::Addressable& addressable) -> Bool {
  BAIL_IF(!complete_value(program, addressable.get_type()));
  Core::Option<Ttx::Lexical::Anchor> anchor;
  addressable.bind<Tetrodotoxin::Source::Declaration>().visit(
      [&](const Tetrodotoxin::Source::Declaration::Handle& declaration) {
        anchor = declaration.get_anchor();
      },
      [](Ttx::Semantic::Binding::Failure) {});
  return !anchor || program.get_debug().field(addressable, *anchor);
}

auto Llvm::Lowering::Types::prepare(
    Llvm::Module::Program& program,
    const Model::Callable& callable) -> Bool {
  return reserve(program, callable) && complete(program, callable);
}

auto Llvm::Lowering::Types::reserve(
    Llvm::Module::Program& program,
    const Model::Callable& callable) -> Bool {
  return reserve_layout(program, callable.get_parameters()) &&
         reserve_layout(program, callable.get_results());
}

auto Llvm::Lowering::Types::complete(
    Llvm::Module::Program& program,
    const Model::Callable& callable) -> Bool {
  return complete_layout(program, callable.get_parameters()) &&
         complete_layout(program, callable.get_results());
}

auto Llvm::Lowering::Types::reserve_declaration(
    Llvm::Module::Program& program,
    const Model::Type& type) -> Bool {
  return reserve_value(program, type);
}

auto Llvm::Lowering::Types::complete_declaration(
    Llvm::Module::Program& program,
    const Model::Type& type) -> Bool {
  BAIL_IF(!complete_value(program, type));
  auto enumeration = type.select<Library::Language::Types::Enumeration>();
  if (!enumeration) {
    return True;
  }
  auto storage = enumeration->get_storage_type();
  BAIL_IF(!storage);
  for (Count index = 0; index < enumeration->get_case_count(); index++) {
    const Ttx::Concept::Abstract& declaration =
        enumeration->get_cases().get_data()[index].get();
    auto value = enumeration->get_case_value(index);
    BAIL_IF(!value);
    Bool completed = storage->is<Model::Types::Signed>()
                         ? program.get_debug().signed_enumerator(
                               program, type, declaration, S64(*value))
                         : program.get_debug().unsigned_enumerator(
                               program, type, declaration, *value);
    BAIL_IF(!completed);
  }
  return True;
}
