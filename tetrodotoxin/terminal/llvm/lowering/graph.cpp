// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/terminal/llvm/lowering/graph.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "perimortem/memory/dynamic/vector.hpp"

#include "tetrodotoxin/library/language/field.hpp"
#include "tetrodotoxin/library/language/foreign.hpp"
#include "tetrodotoxin/library/language/function.hpp"
#include "tetrodotoxin/library/language/types/composite.hpp"
#include "tetrodotoxin/library/language/types/source.hpp"
#include "tetrodotoxin/library/language/types/structure.hpp"
#include "tetrodotoxin/terminal/llvm/lowering/execution.hpp"
#include "tetrodotoxin/terminal/llvm/lowering/types.hpp"
#include "tetrodotoxin/terminal/llvm/module/body.hpp"
#include "tetrodotoxin/terminal/llvm/module/functions.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin::Terminal;
using namespace Tetrodotoxin::Library::Language;

static auto is_excluded(
    const Model::Callable& callable,
    Core::View::Vector<Ttx::Concept::Reference<const Model::Callable>> excluded)
    -> Bool {
  for (const Ttx::Concept::Reference<const Model::Callable>& candidate :
       excluded) {
    if (&candidate.get() == &callable) {
      return True;
    }
  }
  return False;
}

static auto reserve_callable(
    Llvm::Module::Program& program,
    const Model::Callable& callable) -> Bool {
  auto function = callable.select<Function>();
  if (function) {
    auto reserved = program.get_functions().reserve_function(
        program, *function, function->get_definition());
    return reserved &&
           (!*reserved || Llvm::Lowering::Types::reserve(program, callable));
  }
  auto foreign = callable.select<Foreign::Function>();
  if (foreign) {
    auto reserved = program.get_functions().reserve_foreign(
        program, *foreign, foreign->get_abi(), foreign->get_symbol());
    return reserved &&
           (!*reserved || Llvm::Lowering::Types::reserve(program, callable));
  }
  return Llvm::Lowering::Types::reserve(program, callable);
}

static auto complete_callable(
    Llvm::Module::Program& program,
    const Model::Callable& callable) -> Bool {
  BAIL_IF(!Llvm::Lowering::Types::complete(program, callable));
  return callable.is<Function>() || callable.is<Foreign::Function>()
             ? program.get_functions().complete(program, callable)
             : True;
}

static auto reserve_addressable(
    Llvm::Module::Program& program,
    const Ttx::Model::Addressable& addressable) -> Bool {
  BAIL_IF(!Llvm::Lowering::Types::reserve(program, addressable));
  auto field = addressable.select<Field>();
  if (field && field->get_writability() == Writability::Full) {
    return Bool(program.get_globals().reserve_static(program, *field));
  }
  auto state = addressable.select<Foreign::State>();
  if (state) {
    Bool writable = Bool(
        state->get_definition().get_visibility() ==
        Tetrodotoxin::Language::Visibility::Public);
    return Bool(program.get_globals().reserve_foreign(
        program, *state, state->get_abi(), state->get_name(), writable));
  }
  return True;
}

static auto complete_addressable(
    Llvm::Module::Program& program,
    const Ttx::Model::Addressable& addressable) -> Bool {
  BAIL_IF(!Llvm::Lowering::Types::complete(program, addressable));
  auto field = addressable.select<Field>();
  if (field && field->get_writability() == Writability::Full) {
    return program.get_globals().complete(program, *field) &&
           program.get_debug().global(
               program, *field, field->get_definition(), True, True);
  }
  auto state = addressable.select<Foreign::State>();
  return !state ||
         (program.get_globals().complete(program, *state) &&
          program.get_debug().global(
              program, *state, state->get_definition(), False, False));
}

static auto retain_construction_parameters(
    const Types::Structure& structure,
    Memory::Dynamic::Vector<
        Ttx::Concept::Reference<const Ttx::Model::Addressable>>& parameters)
    -> void {
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
       structure.get_addressables()) {
    auto field = candidate.get().select<Field>();
    if (field && field->get_writability() == Writability::Internal &&
        field->get_definition().is_published()) {
      parameters.insert(*field);
    }
  }
}

static auto reserve_type(
    Llvm::Module::Program& program,
    const Model::Type& type,
    Core::View::Vector<Ttx::Concept::Reference<const Model::Callable>> excluded)
    -> Bool {
  BAIL_IF(!Llvm::Lowering::Types::reserve_declaration(program, type));
  auto composite = type.select<Types::Composite>();
  if (composite) {
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         composite->get_types()) {
      auto nested = candidate.get().select<Model::Type>();
      BAIL_IF(nested && !reserve_type(program, *nested, excluded));
    }
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         composite->get_addressables()) {
      auto addressable = candidate.get().select<Model::Memory>();
      BAIL_IF(addressable && !reserve_addressable(program, *addressable));
    }
  }
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
       type.get_callables()) {
    auto callable = candidate.get().select<Model::Callable>();
    BAIL_IF(
        callable && !is_excluded(*callable, excluded) &&
        !reserve_callable(program, *callable));
  }

  auto structure = type.select<Types::Structure>();
  if (structure && !structure->get_layout().is_empty() &&
      structure->is_externally_reachable(*structure)) {
    Memory::Dynamic::Vector<
        Ttx::Concept::Reference<const Ttx::Model::Addressable>>
        parameters;
    retain_construction_parameters(*structure, parameters);
    BAIL_IF(!program.get_functions().reserve_construction(
        program, *structure, structure->has_initialization_provider(),
        parameters.get_view()));
  }

  auto source = type.select<Types::Source>();
  if (source) {
    for (const Ttx::Concept::Reference<Foreign::State>& state :
         source->get_foreign().get_states()) {
      BAIL_IF(!reserve_addressable(program, state.get()));
    }
    for (const Ttx::Concept::Reference<Foreign::Function>& function :
         source->get_foreign().get_functions()) {
      BAIL_IF(!reserve_callable(program, function.get()));
    }
  }
  return True;
}

static auto complete_type(
    Llvm::Module::Program& program,
    const Model::Type& type,
    Core::View::Vector<Ttx::Concept::Reference<const Model::Callable>> excluded)
    -> Bool {
  BAIL_IF(!Llvm::Lowering::Types::complete_declaration(program, type));
  auto composite = type.select<Types::Composite>();
  if (composite) {
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         composite->get_types()) {
      auto nested = candidate.get().select<Model::Type>();
      BAIL_IF(nested && !complete_type(program, *nested, excluded));
    }
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         composite->get_addressables()) {
      auto addressable = candidate.get().select<Model::Memory>();
      BAIL_IF(addressable && !complete_addressable(program, *addressable));
    }
  }
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
       type.get_callables()) {
    auto callable = candidate.get().select<Model::Callable>();
    BAIL_IF(
        callable && !is_excluded(*callable, excluded) &&
        !complete_callable(program, *callable));
  }
  auto structure = type.select<Types::Structure>();
  if (structure && !structure->get_layout().is_empty() &&
      structure->is_externally_reachable(*structure) &&
      !program.get_functions().complete_construction(program, *structure)) {
    return False;
  }
  auto source = type.select<Types::Source>();
  if (source) {
    for (const Ttx::Concept::Reference<Foreign::State>& state :
         source->get_foreign().get_states()) {
      BAIL_IF(!complete_addressable(program, state.get()));
    }
    for (const Ttx::Concept::Reference<Foreign::Function>& function :
         source->get_foreign().get_functions()) {
      BAIL_IF(!complete_callable(program, function.get()));
    }
  }
  return True;
}

static auto emit_field(Llvm::Module::Program& program, const Field& field)
    -> Bool {
  if (field.get_writability() != Writability::Full) {
    return True;
  }
  Core::Option<const Model::Pack&> value = field.get_initializer();
  if (!value) {
    auto type = field.get_type().select<Model::Type>();
    BAIL_IF(!type);
    auto created = type->create_default(program.get_arena());
    BAIL_IF(!created);
    value = *created;
  }
  auto initializer = program.get_globals().begin_initializer(program, field);
  BAIL_IF(!initializer);
  Llvm::Module::Body native_body(program, field, *initializer);
  Llvm::Lowering::Execution execution(native_body);
  return execution.lower(*value) &&
         program.get_globals().end_initializer(native_body, field, *value);
}

static auto emit_function(
    Llvm::Module::Program& program,
    const Function& function) -> Bool {
  auto source_body = function.get_body();
  auto lowering = program.get_functions().begin_body(program, function);
  BAIL_IF(!source_body || !lowering);
  Llvm::Module::Body native_body(
      program, function, lowering->get_function(), lowering->get_callable(),
      lowering->get_sret(), lowering->get_sret_type());
  BAIL_IF(!program.get_functions().bind_parameters(native_body, function));
  Llvm::Lowering::Execution execution(native_body);
  const Llvm::Emission::ControlFlow& control = execution.get_control_flow();
  BAIL_IF(!control.begin_function(function, function.get_definition()));
  const Ttx::Concept::Layout& parameters = function.get_parameters();
  for (Count index = 0; index < parameters.get_size(); index++) {
    auto entry = parameters.get_abstract(index);
    auto parameter = entry ? entry->select<Ttx::Model::Addressable>()
                           : Core::Option<const Ttx::Model::Addressable&>();
    auto anchor = function.get_parameter_anchor(index);
    BAIL_IF(
        !parameter ||
        !control.parameter(
            *parameter,
            anchor ? *anchor : function.get_definition().get_anchor(), index));
  }
  return execution.lower(*source_body) && control.end_function() &&
         program.get_functions().end_body(native_body, function);
}

static auto emit_structure(
    Llvm::Module::Program& program,
    const Types::Structure& structure) -> Bool {
  if (structure.get_layout().is_empty() ||
      !structure.is_externally_reachable(structure)) {
    return True;
  }
  Memory::Dynamic::Vector<Llvm::Module::Functions::ConstructionField> fields;
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
       structure.get_addressables()) {
    auto field = candidate.get().select<Field>();
    if (!field || field->get_writability() != Writability::Internal) {
      continue;
    }
    auto value = field->get_initializer();
    if (!value) {
      auto type = field->get_type().select<Model::Type>();
      BAIL_IF(!type);
      auto created = type->create_default(program.get_arena());
      BAIL_IF(!created);
      value = *created;
    }
    BAIL_IF(!value);
    fields.insert(
        Llvm::Module::Functions::ConstructionField(
            *field, *value, field->get_definition().is_published()));
  }
  if (!program.get_functions().lower_construction(
          program, structure, fields.get_view())) {
    Perimortem::Core::Diagnostics::Log::Message<256> message(
        Perimortem::Core::Diagnostics::Log::Level::Error,
        Perimortem::Core::Diagnostics::Source());
    message << "LLVM could not emit construction for Library Type `"_view
            << structure.get_name() << "`."_view;
    return False;
  }
  return True;
}

static auto emit_type(
    Llvm::Module::Program& program,
    const Model::Type& type,
    Core::View::Vector<Ttx::Concept::Reference<const Model::Callable>> excluded)
    -> Bool {
  auto composite = type.select<Types::Composite>();
  if (composite) {
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& declaration :
         composite->get_declarations()) {
      auto nested = declaration.get().select<Model::Type>();
      auto addressable = declaration.get().select<Model::Memory>();
      auto callable = declaration.get().select<Model::Callable>();
      if (nested && !emit_type(program, *nested, excluded)) {
        return False;
      }
      Core::Option<Field&> field;
      if (addressable) {
        field = addressable->select<Field>();
      }
      if (field && !emit_field(program, *field)) {
        return False;
      }
      Core::Option<Function&> function;
      if (callable) {
        function = callable->select<Function>();
      }
      if (function && !is_excluded(*function, excluded) &&
          !emit_function(program, *function)) {
        return False;
      }
    }
  }
  auto structure = type.select<Types::Structure>();
  return !structure || emit_structure(program, *structure);
}

auto Llvm::Lowering::Graph::lower(
    Llvm::Module::Program& program,
    const Tetrodotoxin::Library::Language::Monograph& monograph,
    Core::View::Vector<Ttx::Concept::Reference<const Model::Callable>> excluded)
    -> Bool {
  const Tetrodotoxin::Library::Language::Types::Source& source =
      monograph.get_source();
  if (!reserve_type(program, source, excluded)) {
    Perimortem::Core::Diagnostics::Log::error(
        "LLVM could not reserve the completed Library graph."_view);
    return False;
  }
  if (!complete_type(program, source, excluded)) {
    Perimortem::Core::Diagnostics::Log::error(
        "LLVM could not complete target facts for the Library graph."_view);
    return False;
  }
  if (!emit_type(program, source, excluded)) {
    Perimortem::Core::Diagnostics::Log::error(
        "LLVM could not emit the completed Library graph."_view);
    return False;
  }
  return True;
}

auto Llvm::Lowering::Graph::prepare(
    Llvm::Module::Program& program,
    const Tetrodotoxin::Library::Language::Model::Callable& callable) -> Bool {
  return reserve_callable(program, callable) &&
         complete_callable(program, callable);
}

auto Llvm::Lowering::Graph::prepare(
    Llvm::Module::Program& program,
    const Ttx::Model::Addressable& addressable) -> Bool {
  return reserve_addressable(program, addressable) &&
         complete_addressable(program, addressable);
}
