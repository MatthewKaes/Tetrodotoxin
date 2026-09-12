// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/terminal/spirv/layout.hpp"

#include "perimortem/core/math.hpp"

#include "tetrodotoxin/language/attribute.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/types/real.hpp"
#include "tetrodotoxin/library/language/model/types/value.hpp"
#include "tetrodotoxin/library/language/types/structure.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin;

static auto vector_capability(
    const Library::Language::Types::Structure& structure) -> Bool {
  auto attributes = structure.get_definition().get_attributes();
  for (Count index = 0; index < attributes.get_size(); index++) {
    const Tetrodotoxin::Language::Attribute& attribute =
        attributes.get_data()[index];
    const Core::View::Bytes* value =
        attribute.get_value().find<Core::View::Bytes>();
    if (attribute.get_key() == "capability"_view && value &&
        *value == "vector"_view) {
      return True;
    }
  }
  return False;
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

auto Terminal::Spirv::Layout::get_vector_components(
    const Library::Language::Model::Type& type) -> Core::Option<Count> {
  auto structure = type.select<Library::Language::Types::Structure>();
  BAIL_IF(!structure || !vector_capability(*structure));
  Count components = structure->get_layout().get_size();
  BAIL_IF(components < 2 || components > 4);
  for (Count index = 0; index < components; index++) {
    auto semantic = structure->get_layout().get_abstract(index);
    auto selected = semantic
                        ? select_type(*semantic)
                        : Core::Option<const Library::Language::Model::Type&>();
    auto real =
        selected ? selected->select<Library::Language::Model::Types::Real>()
                 : Core::Option<const Library::Language::Model::Types::Real&>();
    BAIL_IF(!real || real->get_width() != 32);
  }
  return components;
}

static auto align_up(Count value, Count alignment) -> Count {
  return (value + alignment - 1) / alignment * alignment;
}

auto Terminal::Spirv::Layout::measure(
    const Library::Language::Model::Type& type) -> Core::Option<Measurement> {
  auto value = type.select<Library::Language::Model::Types::Value>();
  if (value) {
    BAIL_IF(value->get_width() == 0 || (value->get_width() & 7) != 0);
    Count size = value->get_width() / 8;
    return Measurement(size, size);
  }
  auto components = get_vector_components(type);
  if (components) {
    Count size = sizeof(R32) * *components;
    Count alignment = sizeof(R32) * (*components == 2 ? 2 : 4);
    return Measurement(size, alignment);
  }
  auto structure = type.select<Library::Language::Types::Structure>();
  BAIL_IF(!structure || structure->get_layout().is_empty());
  Count size = 0;
  Count alignment = 1;
  for (Count index = 0; index < structure->get_layout().get_size(); index++) {
    auto semantic = structure->get_layout().get_abstract(index);
    auto selected = semantic
                        ? select_type(*semantic)
                        : Core::Option<const Library::Language::Model::Type&>();
    auto field = selected ? measure(*selected) : Core::Option<Measurement>();
    BAIL_IF(!field);
    size = align_up(size, field->get_alignment()) + field->get_size();
    alignment = Core::Math::max(alignment, field->get_alignment());
  }
  return Measurement(align_up(size, alignment), alignment);
}
