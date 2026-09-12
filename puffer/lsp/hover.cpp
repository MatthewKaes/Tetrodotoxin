// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "puffer/lsp/hover.hpp"

#include "perimortem/memory/managed/bytes.hpp"

#include "perimortem/serialization/json/blueprint.hpp"
#include "perimortem/serialization/stream/textual.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/language/import.hpp"
#include "ttx/concept/constant.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/model/addressable.hpp"
#include "ttx/model/callable.hpp"
#include "ttx/model/type.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;

static auto append_name(
    Serialization::Stream::Textual<Memory::Managed::Bytes>& output,
    Core::View::Bytes name) -> void {
  constexpr Core::View::Bytes hexadecimal = "0123456789ABCDEF"_view;
  Bool textual = !name.is_empty();
  for (Count index = 0; index < name.get_size(); index++) {
    textual &= name[index] >= 0x20 && name[index] <= 0x7e && name[index] != '`';
  }
  if (textual) {
    output << name;
    return;
  }

  output << "$["_view;
  for (Count index = 0; index < name.get_size(); index++) {
    if (index != 0) {
      output << " "_view;
    }
    output << hexadecimal.slice(name[index] >> 4, 1)
           << hexadecimal.slice(name[index] & 0x0f, 1);
  }
  output << "]"_view;
}

static auto append_type(
    Serialization::Stream::Textual<Memory::Managed::Bytes>& output,
    const Abstract& semantic) -> void {
  auto type = semantic.select<Ttx::Model::Type>();
  const Abstract* answer = &semantic;
  auto addressable = semantic.select<Ttx::Model::Addressable>();
  if (!type && addressable) {
    answer = &addressable->get_type();
    type = answer->select<Ttx::Model::Type>();
  } else if (!type) {
    answer = &semantic.get_type();
    type = answer->select<Ttx::Model::Type>();
  }
  if (!type && !answer->is<Unknown>() && !answer->is<None>()) {
    type = answer->resolve().select<Ttx::Model::Type>();
  }
  if (type) {
    append_name(output, type->get_name());
  } else if (answer->is<Unknown>()) {
    output << "Unknown"_view;
  } else {
    output << "None"_view;
  }
}

static auto append_layout(
    Serialization::Stream::Textual<Memory::Managed::Bytes>& output,
    const Layout& layout) -> void {
  output << "["_view;
  for (Count index = 0; index < layout.get_size(); index++) {
    if (index != 0) {
      output << ", "_view;
    }
    auto entry = layout.get_abstract(index);
    if (!entry) {
      output << "Unknown"_view;
      continue;
    }
    auto name = layout.get_name(index);
    if (name && !name->is_empty()) {
      output << "."_view;
      append_name(output, *name);
      output << " : "_view;
    }
    append_type(output, *entry);
  }
  output << "]"_view;
}

static auto append_identity(
    Serialization::Stream::Textual<Memory::Managed::Bytes>& output,
    const Abstract& semantic) -> Bool {
  auto imported = semantic.select<Tetrodotoxin::Language::Import>();
  if (imported) {
    output << "import "_view;
    append_name(output, imported->get_name());
    const Abstract& type = imported->get_type();
    if (!type.is<Unknown>() && !type.is<None>()) {
      output << " = "_view;
      append_name(output, type.get_name());
    }
    return True;
  }

  // A concrete declaration can retain its authored spelling while resolving
  // to another semantic value. Ask for that declaration's own facts rather
  // than testing whether its reference machinery is an Alias.
  Bool declaration = False;
  if (&semantic.resolve() != &semantic) {
    semantic.bind<Tetrodotoxin::Language::Definition>().visit(
        [&](const Tetrodotoxin::Language::Definition::Handle& definition) {
          append_name(output, definition.get_name());
          const Abstract& value = semantic.resolve();
          if (!value.is<Unknown>() && !value.is<None>()) {
            output << " = "_view;
            append_name(output, value.get_name());
          }
          declaration = True;
        },
        [](Binding::Failure) {});
  }

  if (declaration) {
    return True;
  }

  auto callable = semantic.select<Ttx::Model::Callable>();
  if (callable) {
    output << "func "_view;
    append_name(output, callable->get_name());
    append_layout(output, callable->get_parameters());
    output << " -> "_view;
    append_layout(output, callable->get_results());
    return True;
  }

  auto addressable = semantic.select<Ttx::Model::Addressable>();
  if (addressable) {
    append_name(output, addressable->get_name());
    output << " : "_view;
    append_type(output, *addressable);
    return True;
  }

  if (semantic.is<Constant>()) {
    output << "const "_view;
    append_name(output, semantic.get_name());
    return True;
  }
  if (semantic.is<Ttx::Model::Type>()) {
    output << "Type "_view;
    append_name(output, semantic.get_name());
    return True;
  }
  if (semantic.get_name().is_empty()) {
    return False;
  }
  append_name(output, semantic.get_name());
  return True;
}

auto Puffer::Lsp::semantic_hover(
    Memory::Allocator::Arena& arena,
    const Abstract& semantic) -> Serialization::Json::Node {
  Memory::Managed::Bytes buffer(arena);
  Serialization::Stream::Textual<Memory::Managed::Bytes> output(buffer);
  output << "```tetrodotoxin\n"_view;
  if (!append_identity(output, semantic)) {
    return Serialization::Json::Node();
  }
  output << "\n```"_view;

  const Documentation& documentation = semantic.get_documentation();
  if (!documentation.is_empty()) {
    output << "\n\n"_view;
    for (Count index = 0; index < documentation.line_count(); index++) {
      if (index != 0) {
        output << "\n"_view;
      }
      output << documentation.get_line(index);
    }
  }

  return Serialization::Json::Blueprint{
    {
      {"contents"_view,
       {
         {"kind"_view, "markdown"_view},
         {"value"_view, buffer.get_view()},
       }},
    }}.construct(arena);
}
