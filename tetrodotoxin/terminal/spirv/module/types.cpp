// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/terminal/spirv/module/types.hpp"

#include "perimortem/core/math.hpp"

#include "tetrodotoxin/library/language/function.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/types/flag.hpp"
#include "tetrodotoxin/library/language/model/types/real.hpp"
#include "tetrodotoxin/library/language/model/types/signed.hpp"
#include "tetrodotoxin/library/language/model/types/unsigned.hpp"
#include "tetrodotoxin/library/language/model/types/value.hpp"
#include "tetrodotoxin/library/language/types/structure.hpp"
#include "tetrodotoxin/terminal/spirv/layout.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Tetrodotoxin;
using namespace Tetrodotoxin::Terminal::Spirv;

Module::Types::Types(Ids& ids)
    : ids(ids), void_id(ids.take()), function_id(ids.take()) {}

static auto sample_result(const Library::Language::Model::Type& type)
    -> Core::Option<const Library::Language::Model::Type&> {
  for (const Ttx::Concept::Reference<Ttx::Concept::Abstract>& candidate :
       type.get_callables()) {
    auto function = candidate.get().select<Library::Language::Function>();
    if (!function) {
      continue;
    }
    Bool sample = False;
    for (const Tetrodotoxin::Language::Attribute& attribute :
         function->get_definition().get_attributes()) {
      const Core::View::Bytes* value =
          attribute.get_value().find<Core::View::Bytes>();
      sample |= attribute.get_key() == "intrinsic"_view && value &&
                *value == "sample_2d"_view;
    }
    if (!sample || function->get_results().get_size() != 1) {
      continue;
    }
    auto result = function->get_results().get_abstract(0);
    auto selected = result
                        ? Module::Types::select(*result)
                        : Core::Option<const Library::Language::Model::Type&>();
    BAIL_IF(!selected);
    return *selected;
  }
  return {};
}

auto Module::Types::find_resource(const Library::Language::Model::Type& type)
    const -> Core::Option<const Resource&> {
  for (const Resource& resource : resources.get_view()) {
    if (&resource.type.get() == &type) {
      return resource;
    }
  }
  return {};
}

auto Module::Types::select(const Abstract& semantic)
    -> Core::Option<const Library::Language::Model::Type&> {
  auto addressable = semantic.select<Ttx::Model::Addressable>();
  const Abstract& answer = addressable ? addressable->get_type() : semantic;
  auto direct = answer.select<Library::Language::Model::Type>();
  return direct ? direct
                : answer.resolve().select<Library::Language::Model::Type>();
}

auto Module::Types::get_id(const Library::Language::Model::Type& type) const
    -> Core::Option<U32> {
  auto resource = find_resource(type);
  if (resource) {
    return resource->sampled_id;
  }
  for (const Entry& entry : entries.get_view()) {
    if (&entry.type.get() == &type) {
      return entry.id;
    }
  }
  return {};
}

auto Module::Types::collect(const Library::Language::Model::Type& type)
    -> Bool {
  if (get_id(type)) {
    return True;
  }
  for (const Library::Language::Model::Type* active : visiting.get_view()) {
    BAIL_IF(active == &type);
  }

  auto value = type.select<Library::Language::Model::Types::Value>();
  if (value) {
    auto real = type.select<Library::Language::Model::Types::Real>();
    BAIL_IF(
        real ? real->get_width() != 32 && real->get_width() != 64
             : value->get_width() != 32);
    for (const Entry& entry : entries.get_view()) {
      auto existing =
          entry.type.get().select<Library::Language::Model::Types::Value>();
      if (existing && value->is_equivalent(*existing)) {
        entries.insert(Entry(type, entry.id));
        return True;
      }
    }
    entries.insert(Entry(type, ids.take()));
    return True;
  }

  auto structure = type.select<Library::Language::Types::Structure>();
  BAIL_IF(!structure || structure->get_layout().is_empty());
  visiting.insert(&type);
  const Ttx::Concept::Layout& layout = structure->get_layout();
  for (Count index = 0; index < layout.get_size(); index++) {
    auto semantic = layout.get_abstract(index);
    auto member = semantic
                      ? select(*semantic)
                      : Core::Option<const Library::Language::Model::Type&>();
    BAIL_IF(!member || !collect(*member));
  }
  visiting.remove(visiting.get_size() - 1);
  entries.insert(Entry(type, ids.take()));
  return True;
}

auto Module::Types::requires_float64() const -> Bool {
  for (const Entry& entry : entries.get_view()) {
    auto real =
        entry.type.get().select<Library::Language::Model::Types::Real>();
    if (real && real->get_width() == 64) {
      return True;
    }
  }
  return False;
}

auto Module::Types::collect_resource(const Library::Language::Model::Type& type)
    -> Bool {
  if (find_resource(type)) {
    return True;
  }
  auto sampled = sample_result(type);
  auto structure =
      sampled ? sampled->select<Library::Language::Types::Structure>()
              : Core::Option<const Library::Language::Types::Structure&>();
  auto components =
      sampled ? Terminal::Spirv::Layout::get_vector_components(*sampled)
              : Core::Option<Count>();
  BAIL_IF(
      !sampled || !structure || !components || *components != 4 ||
      !collect(*sampled));
  auto component_semantic = structure->get_layout().get_abstract(0);
  auto component = component_semantic
                       ? select(*component_semantic)
                       : Core::Option<const Library::Language::Model::Type&>();
  auto real =
      component ? component->select<Library::Language::Model::Types::Real>()
                : Core::Option<const Library::Language::Model::Types::Real&>();
  BAIL_IF(
      !component || !real || real->get_width() != 32 || !get_id(*component));
  resources.insert(
      Resource(type, *sampled, ids.take(), ids.take(), ids.take()));
  return True;
}

auto Module::Types::collect_pointer(
    const Library::Language::Model::Type& type,
    Assembler::SpirV::StorageClass storage) -> Bool {
  BAIL_IF(!collect(type));
  if (get_pointer_id(type, storage)) {
    return True;
  }
  pointers.insert(Pointer(type, storage, ids.take()));
  return True;
}

auto Module::Types::get_pointer_id(
    const Library::Language::Model::Type& type,
    Assembler::SpirV::StorageClass storage) const -> Core::Option<U32> {
  for (const Pointer& pointer : pointers.get_view()) {
    if (&pointer.type.get() == &type && pointer.storage == storage) {
      return pointer.id;
    }
  }
  return {};
}

auto Module::Types::get_resource_pointer_id(
    const Library::Language::Model::Type& type) const -> Core::Option<U32> {
  for (const Resource& resource : resources.get_view()) {
    if (&resource.type.get() == &type) {
      return resource.pointer_id;
    }
  }
  return {};
}

auto Module::Types::get_unsigned_32_id() const -> Core::Option<U32> {
  for (const Entry& entry : entries.get_view()) {
    auto type =
        entry.type.get().select<Library::Language::Model::Types::Unsigned>();
    if (type && type->get_width() == 32) {
      return entry.id;
    }
  }
  return {};
}

auto Module::Types::emit(Assembler::SpirV& assembler) const -> Bool {
  assembler.type_void(void_id);
  auto retained_entries = entries.get_view();
  for (Count entry_index = 0; entry_index < retained_entries.get_size();
       entry_index++) {
    const Entry& entry = retained_entries.get_data()[entry_index];
    Bool emitted_id = False;
    for (Count prior = 0; prior < entry_index; prior++) {
      emitted_id |= retained_entries.get_data()[prior].id == entry.id;
    }
    if (emitted_id) {
      continue;
    }
    const Library::Language::Model::Type& type = entry.type.get();
    if (type.is<Library::Language::Model::Types::Flag>()) {
      assembler.type_bool(entry.id);
      continue;
    }
    auto real = type.select<Library::Language::Model::Types::Real>();
    if (real) {
      assembler.type_float(entry.id, U32(real->get_width()));
      continue;
    }
    auto signed_type = type.select<Library::Language::Model::Types::Signed>();
    if (signed_type) {
      assembler.type_int(entry.id, U32(signed_type->get_width()), True);
      continue;
    }
    auto unsigned_type =
        type.select<Library::Language::Model::Types::Unsigned>();
    if (unsigned_type) {
      assembler.type_int(entry.id, U32(unsigned_type->get_width()), False);
      continue;
    }

    auto structure = type.select<Library::Language::Types::Structure>();
    BAIL_IF(!structure);
    auto vector_components =
        Terminal::Spirv::Layout::get_vector_components(type);
    if (vector_components) {
      const Ttx::Concept::Layout& layout = structure->get_layout();
      auto component_semantic = layout.get_abstract(0);
      auto component =
          component_semantic
              ? select(*component_semantic)
              : Core::Option<const Library::Language::Model::Type&>();
      auto component_id = component ? get_id(*component) : Core::Option<U32>();
      auto component_value =
          component
              ? component->select<Library::Language::Model::Types::Value>()
              : Core::Option<const Library::Language::Model::Types::Value&>();
      BAIL_IF(!component || !component_value || !component_id);
      for (Count index = 1; index < layout.get_size(); index++) {
        auto semantic = layout.get_abstract(index);
        auto selected =
            semantic ? select(*semantic)
                     : Core::Option<const Library::Language::Model::Type&>();
        auto selected_value =
            selected
                ? selected->select<Library::Language::Model::Types::Value>()
                : Core::Option<const Library::Language::Model::Types::Value&>();
        BAIL_IF(
            !selected_value ||
            !component_value->is_equivalent(*selected_value));
      }
      assembler.type_vector(entry.id, *component_id, U32(*vector_components));
      continue;
    }
    Memory::Dynamic::Vector<U32> members;
    const Ttx::Concept::Layout& layout = structure->get_layout();
    for (Count index = 0; index < layout.get_size(); index++) {
      auto semantic = layout.get_abstract(index);
      auto member = semantic
                        ? select(*semantic)
                        : Core::Option<const Library::Language::Model::Type&>();
      auto id = member ? get_id(*member) : Core::Option<U32>();
      BAIL_IF(!id);
      members.insert(*id);
    }
    assembler.type_struct(entry.id, members.get_view());
  }

  for (const Resource& resource : resources.get_view()) {
    auto sampled =
        resource.sampled.get().select<Library::Language::Types::Structure>();
    BAIL_IF(!sampled);
    auto component_semantic = sampled->get_layout().get_abstract(0);
    auto component =
        component_semantic
            ? select(*component_semantic)
            : Core::Option<const Library::Language::Model::Type&>();
    auto component_id = component ? get_id(*component) : Core::Option<U32>();
    BAIL_IF(!component_id);
    assembler.type_image(
        resource.image_id, *component_id, Assembler::SpirV::Dim::D2, 0, 0, 0, 1,
        Assembler::SpirV::ImageFormat::Unknown);
    assembler.type_sampled_image(resource.sampled_id, resource.image_id);
    assembler.type_pointer(
        resource.pointer_id, Assembler::SpirV::StorageClass::UniformConstant,
        resource.sampled_id);
  }

  for (const Pointer& pointer : pointers.get_view()) {
    auto type_id = get_id(pointer.type.get());
    BAIL_IF(!type_id);
    assembler.type_pointer(pointer.id, pointer.storage, *type_id);
  }
  assembler.type_function(function_id, void_id);
  return True;
}

auto Module::Types::decorate_push(
    Assembler::SpirV& assembler,
    const Library::Language::Model::Type& type) const -> Bool {
  auto structure = type.select<Library::Language::Types::Structure>();
  auto id = get_id(type);
  BAIL_IF(
      !structure || !id ||
      Terminal::Spirv::Layout::get_vector_components(type));
  Count offset = 0;
  for (Count index = 0; index < structure->get_layout().get_size(); index++) {
    auto semantic = structure->get_layout().get_abstract(index);
    auto field = semantic
                     ? select(*semantic)
                     : Core::Option<const Library::Language::Model::Type&>();
    auto layout = field ? Terminal::Spirv::Layout::measure(*field)
                        : Core::Option<Terminal::Spirv::Layout::Measurement>();
    BAIL_IF(!field || !layout);
    Count alignment = layout->get_alignment();
    offset = (offset + alignment - 1) / alignment * alignment;
    BAIL_IF(offset > U32(-1));
    assembler.member_decorate(
        *id, U32(index), Assembler::SpirV::Decoration::Offset, U32(offset));
    offset += layout->get_size();
  }
  return True;
}
