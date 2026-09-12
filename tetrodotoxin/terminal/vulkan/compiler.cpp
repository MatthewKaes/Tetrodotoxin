// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/terminal/vulkan/compiler.hpp"

#include "perimortem/core/math.hpp"

#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/library/language/function.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/types/real.hpp"
#include "tetrodotoxin/library/language/model/types/value.hpp"
#include "tetrodotoxin/library/language/types/structure.hpp"
#include "tetrodotoxin/render/language/binding.hpp"
#include "tetrodotoxin/shader/language/binding.hpp"
#include "tetrodotoxin/terminal/spirv/layout.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin;

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

static auto text_attribute(
    Core::View::Vector<Tetrodotoxin::Language::Attribute> attributes,
    Core::View::Bytes name) -> Core::Option<Core::View::Bytes> {
  auto selected = attribute(attributes, name);
  const Core::View::Bytes* value =
      selected ? selected->get_value().find<Core::View::Bytes>() : nullptr;
  return value ? Core::Option<Core::View::Bytes>(*value)
               : Core::Option<Core::View::Bytes>();
}

static auto stage_of(const Library::Language::Function& function)
    -> Core::Option<Terminal::Vulkan::Products::Stage> {
  Core::View::Bytes name = function.get_name();
  auto capability = text_attribute(
      function.get_definition().get_attributes(), "capability"_view);
  if (capability) {
    name = *capability;
  }
  if (name == "vertex"_view) {
    return Terminal::Vulkan::Products::Stage::Vertex;
  }
  if (name == "fragment"_view) {
    return Terminal::Vulkan::Products::Stage::Pixel;
  }
  return {};
}

static auto align_up(Count value, Count alignment) -> Count {
  return (value + alignment - 1) / alignment * alignment;
}

static auto select_type(const Ttx::Concept::Abstract& semantic)
    -> Core::Option<const Library::Language::Model::Type&> {
  auto addressable = semantic.select<Ttx::Model::Addressable>();
  const Ttx::Concept::Abstract& answer =
      addressable ? addressable->get_type() : semantic;
  auto direct = answer.select<Library::Language::Model::Type>();
  return direct ? direct
                : answer.resolve().select<Library::Language::Model::Type>();
}

static auto uses_float64(const Library::Language::Model::Type& type) -> Bool {
  auto real = type.select<Library::Language::Model::Types::Real>();
  if (real) {
    return real->get_width() == 64;
  }
  auto structure = type.select<Library::Language::Types::Structure>();
  if (!structure) {
    return False;
  }
  for (Count index = 0; index < structure->get_layout().get_size(); index++) {
    auto semantic = structure->get_layout().get_abstract(index);
    auto field = semantic
                     ? select_type(*semantic)
                     : Core::Option<const Library::Language::Model::Type&>();
    if (field && uses_float64(*field)) {
      return True;
    }
  }
  return False;
}

auto Terminal::Vulkan::Compiler::compile_module(
    Memory::Allocator::Arena& arena,
    const Terminal::Spirv::Request& request) const
    -> Utility::Result<Terminal::Spirv::Products, Terminal::Spirv::Failure> {
  return Terminal::Spirv::Compiler().compile(arena, request);
}

auto Terminal::Vulkan::Compiler::describe(
    Memory::Allocator::Arena& arena,
    const Shader::Language::Program& program,
    Core::View::Bytes symbol) const -> Core::Option<Products> {
  auto contract = program.get_contract();
  BAIL_IF(symbol.is_empty() || !contract);

  Memory::Managed::Vector<Products::Entry> entries(arena);
  const Library::Language::Function* vertex = nullptr;
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
       program.get_callables()) {
    auto function = candidate.get().select<Library::Language::Function>();
    auto stage =
        function ? stage_of(*function) : Core::Option<Products::Stage>();
    BAIL_IF(!function || !stage);
    entries.insert(Products::Entry{*stage, function->get_name()});
    if (*stage == Products::Stage::Vertex) {
      BAIL_IF(vertex);
      vertex = &*function;
    }
  }
  BAIL_IF(entries.get_size() != 2 || !vertex);

  Memory::Managed::Vector<Products::Descriptor> descriptors(arena);
  Memory::Managed::Vector<Products::HostField> host_fields(arena);
  Count host_size = 0;
  Count host_alignment = 1;
  Count parameters_offset = 0;
  Count parameters_size = 0;
  Bool requires_float64 = False;
  Count descriptor_slot = 0;
  for (const Shader::Language::Binding& binding : program.get_bindings()) {
    const Library::Language::Field& field = binding.get_field();
    if (binding.get_kind() == Render::Language::Binding::Kind::Resource) {
      BAIL_IF(binding.get_access() != Render::Language::Binding::Access::Read);
      descriptors.insert(
          Products::Descriptor{
            field.get_name(),
            descriptor_slot,
            0,
            Products::Resource::SampledTexture2D,
          });
      descriptor_slot++;
      continue;
    }
    BAIL_IF(binding.get_kind() != Render::Language::Binding::Kind::Push);
    auto structure =
        field.get_type().select<Library::Language::Types::Structure>();
    auto structure_layout =
        structure ? Terminal::Spirv::Layout::measure(*structure)
                  : Core::Option<Terminal::Spirv::Layout::Measurement>();
    BAIL_IF(!structure || !structure_layout);

    Count structure_offset =
        align_up(host_size, structure_layout->get_alignment());
    if (&field == &program.get_parameters_field()) {
      parameters_offset = structure_offset;
      parameters_size = structure_layout->get_size();
      host_fields.insert(
          Products::HostField{
            field.get_name(),
            parameters_offset,
            parameters_size,
            Products::HostRole::Parameter,
          });
    } else {
      Count field_offset = 0;
      for (Count index = 0; index < structure->get_layout().get_size();
           index++) {
        auto semantic = structure->get_layout().get_abstract(index);
        auto addressable =
            semantic
                ? semantic->select<Library::Language::Model::Memory>()
                : Core::Option<const Library::Language::Model::Memory&>();
        auto type = addressable
                        ? select_type(*addressable)
                        : Core::Option<const Library::Language::Model::Type&>();
        auto layout =
            type ? Terminal::Spirv::Layout::measure(*type)
                 : Core::Option<Terminal::Spirv::Layout::Measurement>();
        BAIL_IF(!addressable || !type || !layout);
        field_offset = align_up(field_offset, layout->get_alignment());
        auto projected_field = addressable->select<Library::Language::Field>();
        BAIL_IF(!projected_field);
        Products::HostRole role;
        if (projected_field->get_name() == "transform_x"_view) {
          role = Products::HostRole::TransformX;
        } else if (projected_field->get_name() == "transform_y"_view) {
          role = Products::HostRole::TransformY;
        } else {
          return {};
        }
        host_fields.insert(
            Products::HostField{
              addressable->get_name(),
              structure_offset + field_offset,
              layout->get_size(),
              role,
            });
        field_offset += layout->get_size();
      }
    }
    requires_float64 |= uses_float64(*structure);
    host_size = structure_offset + structure_layout->get_size();
    host_alignment =
        Core::Math::max(host_alignment, structure_layout->get_alignment());
  }
  host_size = align_up(host_size, host_alignment);
  BAIL_IF(descriptors.is_empty() || host_size == 0 || parameters_size == 0);
  for (Count index = 0; index < descriptors.get_size(); index++) {
    for (Count prior = 0; prior < index; prior++) {
      BAIL_IF(
          descriptors[index].set == descriptors[prior].set &&
          descriptors[index].slot == descriptors[prior].slot);
    }
  }

  Memory::Managed::Vector<Products::VertexInput> vertex_inputs(arena);
  Count stride = 0;
  const auto& parameters = vertex->get_signature().get_parameters();
  for (Count index = 0; index < parameters.get_size(); index++) {
    auto semantic = parameters.get_abstract(index);
    auto type = semantic
                    ? select_type(*semantic)
                    : Core::Option<const Library::Language::Model::Type&>();
    auto components =
        type ? Terminal::Spirv::Layout::get_vector_components(*type)
             : Core::Option<Count>();
    BAIL_IF(!components);
    vertex_inputs.insert(Products::VertexInput{index, *components, stride, 0});
    stride += *components * sizeof(R32);
  }
  for (Count index = 0; index < vertex_inputs.get_size(); index++) {
    vertex_inputs[index].stride = stride;
  }

  return Products(
      symbol, entries.get_view(), descriptors.get_view(),
      vertex_inputs.get_view(), host_fields.get_view(), host_size,
      parameters_offset, parameters_size, requires_float64);
}
