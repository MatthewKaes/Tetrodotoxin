// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/terminal/spirv/module/interface.hpp"

#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/render/language/attributes.hpp"
#include "tetrodotoxin/render/language/stage.hpp"
#include "tetrodotoxin/shader/language/binding.hpp"
#include "tetrodotoxin/terminal/spirv/layout.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin;
using namespace Tetrodotoxin::Terminal::Spirv;

static auto attribute(
    Core::View::Vector<Tetrodotoxin::Language::Attribute> attributes,
    Core::View::Bytes name)
    -> Core::Option<const Tetrodotoxin::Language::Attribute&> {
  for (Count index = 0; index < attributes.get_size(); index++) {
    const Tetrodotoxin::Language::Attribute& selected =
        attributes.get_data()[index];
    if (selected.get_key() == name) {
      return selected;
    }
  }
  return {};
}

static auto execution_model(
    Core::View::Bytes stage_name,
    Core::View::Vector<Tetrodotoxin::Language::Attribute> attributes)
    -> Core::Option<Assembler::SpirV::ExecutionModel> {
  Core::View::Bytes name = stage_name;
  auto capability = attribute(attributes, "capability"_view);
  if (capability) {
    const Core::View::Bytes* selected =
        capability->get_value().find<Core::View::Bytes>();
    BAIL_IF(!selected);
    name = *selected;
  }
  if (name == "vertex"_view) {
    return Assembler::SpirV::ExecutionModel::Vertex;
  }
  if (name == "fragment"_view) {
    return Assembler::SpirV::ExecutionModel::Fragment;
  }
  return {};
}

auto Module::Interface::prepare(const Shader::Language::Program& program)
    -> Bool {
  BAIL_IF(!prepare_bindings(program));
  // Render chooses which Functions are entry points. Starting from its required
  // Stages prevents a helper Function from becoming an accidental GPU entry
  // merely because it shares the Program context.
  auto contract = program.get_contract();
  BAIL_IF(!contract);
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& requirement :
       contract->get_callables()) {
    auto render_stage = requirement.get().select<Render::Language::Stage>();
    BAIL_IF(!render_stage);
    Core::Option<const Library::Language::Function&> function;
    for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
         program.get_callables()) {
      auto selected = candidate.get().select<Library::Language::Function>();
      if (selected && selected->get_name() == render_stage->get_name()) {
        BAIL_IF(function);
        function = *selected;
      }
    }
    BAIL_IF(!function || function->declares_self() || !function->get_body());
    auto model = execution_model(
        render_stage->get_name(),
        render_stage->get_definition().get_attributes());
    BAIL_IF(!model);

    auto& stage = arena.construct<Stage>(arena, *function, *model, ids.take());
    BAIL_IF(
        !prepare_variables(
            stage, function->get_signature().get_parameters(),
            Assembler::SpirV::StorageClass::Input) ||
        !prepare_variables(
            stage, function->get_signature().get_results(),
            Assembler::SpirV::StorageClass::Output));
    stages.insert(&stage);
  }
  return !stages.is_empty();
}

auto Module::Interface::prepare_bindings(
    const Shader::Language::Program& program) -> Bool {
  Count push_index = 0;
  for (const Shader::Language::Binding& binding : program.get_bindings()) {
    const Library::Language::Field& field = binding.get_field();
    auto type = field.get_type().select<Library::Language::Model::Type>();
    BAIL_IF(!type);
    Assembler::SpirV::StorageClass storage;
    if (binding.get_kind() == Render::Language::Binding::Kind::Push) {
      storage = Assembler::SpirV::StorageClass::PushConstant;
      BAIL_IF(!types.collect_pointer(*type, storage));
      if (push_type_id == 0) {
        push_type_id = ids.take();
        push_pointer_id = ids.take();
        push_variable_id = ids.take();
      }
      bindings.insert(Variable(
          field, *type, field.get_name(),
          field.get_definition().get_attributes(), storage, 0, push_index++,
          ids.take()));
      continue;
    } else if (
        binding.get_kind() == Render::Language::Binding::Kind::Resource) {
      BAIL_IF(binding.get_access() != Render::Language::Binding::Access::Read);
      storage = Assembler::SpirV::StorageClass::UniformConstant;
      BAIL_IF(!types.collect_resource(*type));
    } else {
      return False;
    }
    bindings.insert(Variable(
        field, *type, field.get_name(), field.get_definition().get_attributes(),
        storage, ids.take()));
  }
  if (push_type_id != 0) {
    auto shared_index_type = types.get_unsigned_32_id();
    push_index_type_id = shared_index_type ? *shared_index_type : ids.take();
    owns_push_index_type = !shared_index_type;
  }
  return True;
}

auto Module::Interface::is_resource(
    const Ttx::Concept::Abstract& semantic) const -> Bool {
  for (const Variable& binding : bindings.get_view()) {
    if (&binding.semantic.get() == &semantic &&
        binding.storage == Assembler::SpirV::StorageClass::UniformConstant) {
      return True;
    }
  }
  return False;
}

auto Module::Interface::prepare_variables(
    Stage& stage,
    const Library::Language::Model::Layout& layout,
    Assembler::SpirV::StorageClass storage) -> Bool {
  Count location = 0;
  for (Count index = 0; index < layout.get_size(); index++) {
    auto semantic = layout.get_abstract(index);
    auto type = semantic
                    ? Types::select(*semantic)
                    : Core::Option<const Library::Language::Model::Type&>();
    Core::View::Bytes name = layout.get_declared_name(index);
    auto attributes = layout.get_slot_attributes(index);
    BAIL_IF(
        !semantic || !type || name.is_empty() ||
        !types.collect_pointer(*type, storage));
    Bool builtin = Bool(
        (storage == Assembler::SpirV::StorageClass::Output &&
         name == "position"_view) ||
        (storage == Assembler::SpirV::StorageClass::Input &&
         name == "vertex_index"_view));
    Variable variable(
        *semantic, *type, name, attributes, storage, ids.take(), Count(-1), 0,
        builtin ? Count(-1) : location++);
    if (storage == Assembler::SpirV::StorageClass::Input) {
      stage.inputs.insert(variable);
    } else {
      stage.outputs.insert(variable);
    }
  }
  return True;
}

auto Module::Interface::emit_entry_points(Assembler::SpirV& assembler) const
    -> void {
  for (Stage* stage : stages.get_view()) {
    Memory::Dynamic::Vector<U32> variables;
    for (const Variable& input : stage->inputs.get_view()) {
      variables.insert(input.id);
    }
    for (const Variable& output : stage->outputs.get_view()) {
      variables.insert(output.id);
    }
    assembler.entry_point(
        stage->model, stage->id, stage->function.get().get_name(),
        variables.get_view());
    if (stage->model == Assembler::SpirV::ExecutionModel::Fragment) {
      assembler.execution_mode(
          stage->id, Assembler::SpirV::ExecutionMode::OriginUpperLeft);
    }
  }
}

auto Module::Interface::emit_debug(Assembler::SpirV& assembler) const -> void {
  for (const Variable& binding : bindings.get_view()) {
    if (binding.storage == Assembler::SpirV::StorageClass::PushConstant) {
      assembler.member_name(
          push_type_id, U32(binding.push_index), binding.name);
    } else {
      assembler.name(binding.id, binding.name);
    }
  }
  if (push_variable_id != 0) {
    assembler.name(push_variable_id, "push"_view);
  }
  for (Stage* stage : stages.get_view()) {
    assembler.name(stage->id, stage->function.get().get_name());
    for (const Variable& input : stage->inputs.get_view()) {
      assembler.name(input.id, input.name);
    }
    for (const Variable& output : stage->outputs.get_view()) {
      assembler.name(output.id, output.name);
    }
  }
}

auto Module::Interface::decorate(
    Assembler::SpirV& assembler,
    const Variable& variable) -> Bool {
  if (variable.location != Count(-1)) {
    BAIL_IF(variable.location > U32(-1));
    assembler.decorate(
        variable.id, Assembler::SpirV::Decoration::Location,
        U32(variable.location));
    return True;
  }

  Assembler::SpirV::BuiltIn selected;
  if (variable.name == "position"_view) {
    selected = Assembler::SpirV::BuiltIn::Position;
  } else if (variable.name == "vertex_index"_view) {
    selected = Assembler::SpirV::BuiltIn::VertexIndex;
  } else {
    return False;
  }
  assembler.decorate(
      variable.id, Assembler::SpirV::Decoration::BuiltIn, U32(selected));
  return True;
}

auto Module::Interface::emit_annotations(Assembler::SpirV& assembler) const
    -> Bool {
  Count push_offset = 0;
  Count descriptor_slot = 0;
  Memory::Dynamic::Vector<const Library::Language::Model::Type*>
      decorated_push_types;
  for (const Variable& binding : bindings.get_view()) {
    if (binding.storage == Assembler::SpirV::StorageClass::PushConstant) {
      auto layout = Terminal::Spirv::Layout::measure(binding.type.get());
      BAIL_IF(!layout || binding.push_index > U32(-1));
      if (!decorated_push_types.contains(&binding.type.get())) {
        BAIL_IF(!types.decorate_push(assembler, binding.type.get()));
        decorated_push_types.insert(&binding.type.get());
      }
      Count alignment = layout->get_alignment();
      push_offset = (push_offset + alignment - 1) / alignment * alignment;
      BAIL_IF(push_offset > U32(-1));
      assembler.member_decorate(
          push_type_id, U32(binding.push_index),
          Assembler::SpirV::Decoration::Offset, U32(push_offset));
      push_offset += layout->get_size();
      continue;
    }
    assembler.decorate(
        binding.id, Assembler::SpirV::Decoration::DescriptorSet,
        U32(descriptor_slot));
    BAIL_IF(descriptor_slot > U32(-1));
    assembler.decorate(
        binding.id, Assembler::SpirV::Decoration::Binding, U32(0));
    descriptor_slot++;
  }
  if (push_type_id != 0) {
    assembler.decorate(push_type_id, Assembler::SpirV::Decoration::Block);
  }
  for (Stage* stage : stages.get_view()) {
    for (const Variable& input : stage->inputs.get_view()) {
      BAIL_IF(!decorate(assembler, input));
    }
    for (const Variable& output : stage->outputs.get_view()) {
      BAIL_IF(!decorate(assembler, output));
    }
  }
  return True;
}

auto Module::Interface::emit_types(Assembler::SpirV& assembler) const -> Bool {
  if (push_type_id == 0) {
    return True;
  }

  Memory::Dynamic::Vector<U32> members;
  for (const Variable& binding : bindings.get_view()) {
    if (binding.storage != Assembler::SpirV::StorageClass::PushConstant) {
      continue;
    }
    auto type_id = types.get_id(binding.type.get());
    BAIL_IF(!type_id);
    members.insert(*type_id);
  }
  BAIL_IF(members.get_size() == 0);

  if (owns_push_index_type) {
    assembler.type_int(push_index_type_id, 32, False);
  }
  assembler.type_struct(push_type_id, members.get_view());
  assembler.type_pointer(
      push_pointer_id, Assembler::SpirV::StorageClass::PushConstant,
      push_type_id);
  for (const Variable& binding : bindings.get_view()) {
    if (binding.storage == Assembler::SpirV::StorageClass::PushConstant) {
      assembler.constant(
          push_index_type_id, binding.push_index_id, U32(binding.push_index));
    }
  }
  return True;
}

auto Module::Interface::emit_globals(Assembler::SpirV& assembler) const
    -> Bool {
  for (const Variable& binding : bindings.get_view()) {
    if (binding.storage == Assembler::SpirV::StorageClass::UniformConstant) {
      auto pointer = types.get_resource_pointer_id(binding.type.get());
      BAIL_IF(!pointer);
      assembler.variable(*pointer, binding.id, binding.storage);
    } else if (
        binding.storage != Assembler::SpirV::StorageClass::PushConstant) {
      return False;
    }
  }
  if (push_variable_id != 0) {
    assembler.variable(
        push_pointer_id, push_variable_id,
        Assembler::SpirV::StorageClass::PushConstant);
  }
  for (Stage* stage : stages.get_view()) {
    for (const Variable& input : stage->inputs.get_view()) {
      auto pointer = types.get_pointer_id(input.type.get(), input.storage);
      BAIL_IF(!pointer);
      assembler.variable(*pointer, input.id, input.storage);
    }
    for (const Variable& output : stage->outputs.get_view()) {
      auto pointer = types.get_pointer_id(output.type.get(), output.storage);
      BAIL_IF(!pointer);
      assembler.variable(*pointer, output.id, output.storage);
    }
  }
  return True;
}

auto Module::Interface::get_binding_pointer(
    const Variable& binding,
    Assembler::SpirV& assembler) const -> Core::Option<U32> {
  if (binding.storage != Assembler::SpirV::StorageClass::PushConstant) {
    return binding.id;
  }

  auto pointer = types.get_pointer_id(binding.type.get(), binding.storage);
  BAIL_IF(!pointer || binding.push_index_id == 0 || push_variable_id == 0);
  U32 id = ids.take();
  assembler.access_chain(
      *pointer, id, push_variable_id,
      Core::View::Vector<U32>(&binding.push_index_id, 1));
  return id;
}
