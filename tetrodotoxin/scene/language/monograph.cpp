// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/scene/language/monograph.hpp"

#include "tetrodotoxin/library/language/model/memory.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin;

auto Scene::Language::Monograph::create(
    Allocator::Arena& domain,
    const Documentation& documentation,
    const Abstract& language,
    Abstract& context,
    Library::Language::Monograph& library,
    Library::Language::Types::Object& instance) -> Monograph& {
  return domain.construct_from<Monograph>([&]() -> Monograph {
    return Monograph(
        domain, documentation, language, context, library, instance);
  });
}

auto Scene::Language::Monograph::retain_signal(Signal& signal, Cursor& cursor)
    -> Bool {
  if (find_signal(signal.get_name())) {
    cursor.create_expression_error(
        signal.get_anchor(), "Scene Signal name is already occupied."_view);
    return False;
  }
  signals.insert(signal);
  return True;
}

auto Scene::Language::Monograph::retain_restored_signal(Signal& signal)
    -> Bool {
  BAIL_IF(find_signal(signal.get_name()));
  signals.insert(signal);
  return True;
}

auto Scene::Language::Monograph::retain_emission(Emission& emission) -> Bool {
  BAIL_IF(stage != Stage::Authored);
  emissions.insert(emission);
  return True;
}

auto Scene::Language::Monograph::retain_lifecycle(
    Lifecycle role,
    Library::Language::Function& function,
    Cursor& cursor) -> Bool {
  Count index = U8(role);
  BAIL_IF(index >= lifecycle.get_size());
  Option<Library::Language::Function&>& selected = lifecycle.get_data()[index];
  if (selected) {
    cursor.create_expression_error(
        function.get_anchor(),
        "Scene lifecycle role is already implemented by this Scene."_view);
    return False;
  }
  selected = function;
  return True;
}

auto Scene::Language::Monograph::retain_restored_lifecycle(
    Lifecycle role,
    Library::Language::Function& function) -> Bool {
  Count index = U8(role);
  BAIL_IF(index >= lifecycle.get_size() || lifecycle.get_data()[index]);
  lifecycle.get_data()[index] = function;
  return True;
}

auto Scene::Language::Monograph::find_signal(View::Bytes name) const
    -> Option<const Signal&> {
  for (const Reference<Signal>& signal : signals.get_view()) {
    if (signal.get().get_name() == name) {
      return signal.get();
    }
  }
  return {};
}

auto Scene::Language::Monograph::get_lifecycle(Lifecycle role) const
    -> Option<const Library::Language::Function&> {
  Count index = U8(role);
  BAIL_IF(index >= lifecycle.get_size());
  const Option<Library::Language::Function&>& selected =
      lifecycle.get_data()[index];
  return selected ? Option<const Library::Language::Function&>(*selected)
                  : Option<const Library::Language::Function&>();
}

auto Scene::Language::Monograph::get_layer(const Abstract& requested) const
    -> Option<const Tetrodotoxin::Language::Monograph&> {
  auto outer = Tetrodotoxin::Language::Monograph::get_layer(requested);
  if (outer) {
    return *outer;
  }
  return &requested == &library.get_language()
             ? Option<const Tetrodotoxin::Language::Monograph&>(library)
             : Option<const Tetrodotoxin::Language::Monograph&>();
}

auto Scene::Language::Monograph::link(Cursor& cursor) -> Bool {
  if (stage >= Stage::Linked) {
    return True;
  }

  Bool valid = library.link(cursor);
  for (const Reference<Signal>& signal : signals.get_view()) {
    valid &= signal.get().link(cursor, library);
  }
  if (valid) {
    for (const Reference<Emission>& emission : emissions.get_view()) {
      valid &= emission.get().validate(cursor);
    }
    valid &= validate_lifecycle(cursor);
  }
  if (valid) {
    stage = Stage::Linked;
  }
  return valid;
}

auto Scene::Language::Monograph::finalize(Cursor& cursor) -> Bool {
  if (stage == Stage::Finalized) {
    return True;
  }
  BAIL_IF(stage != Stage::Linked || !library.finalize(cursor));
  stage = Stage::Finalized;
  return True;
}

auto Scene::Language::Monograph::link_restored() -> Bool {
  Bool valid = library.link_restored();
  for (const Reference<Signal>& signal : signals.get_view()) {
    valid &= signal.get().link_restored(library);
  }
  valid &= emissions.is_empty() && validate_lifecycle_restored();
  if (valid) {
    stage = Stage::Linked;
  }
  return valid;
}

auto Scene::Language::Monograph::finalize_restored() -> Bool {
  BAIL_IF(stage != Stage::Linked || !library.finalize_restored());
  stage = Stage::Finalized;
  return True;
}

auto Scene::Language::Monograph::resolve_concept(View::Bytes route) const
    -> const Abstract& {
  if (route == "instance"_view) {
    return instance.resolve_concept("instance"_view);
  }
  if (route == "static"_view) {
    return library.get_source().resolve_concept("static"_view);
  }

  auto signal = find_signal(route);
  if (signal) {
    return *signal;
  }

  const Abstract& member = instance.resolve_concept(route);
  if (!member.is<Unknown>() && !member.is<None>()) {
    return member;
  }

  const Abstract& child = library.resolve_concept(route);
  return child.is<Unknown>() || child.is<None>()
             ? Tetrodotoxin::Language::Monograph::resolve_concept(route)
             : child;
}

auto Scene::Language::Monograph::resolve_lexical_context(
    View::Bytes route) const -> const Abstract& {
  auto signal = find_signal(route);
  if (signal) {
    return *signal;
  }

  const Abstract& member = instance.resolve_lexical_context(route);
  if (!member.is<Unknown>() && !member.is<None>()) {
    return member;
  }

  const Abstract& child = library.resolve_lexical_context(route);
  return child.is<Unknown>() || child.is<None>()
             ? Tetrodotoxin::Language::Monograph::resolve_lexical_context(route)
             : child;
}

auto Scene::Language::Monograph::validate_lifecycle(Cursor& cursor) const
    -> Bool {
  auto prepare = get_lifecycle(Lifecycle::Prepare);
  auto pause = get_lifecycle(Lifecycle::Pause);
  auto resume = get_lifecycle(Lifecycle::Resume);
  auto update = get_lifecycle(Lifecycle::Update);
  auto release = get_lifecycle(Lifecycle::Release);
  if (!prepare || !update || !release) {
    cursor.create_error(
        "Scene requires prepare, update, and release lifecycle roles."_view);
    return False;
  }

  Bool valid = True;
  auto validate_empty =
      [&](const Option<const Library::Language::Function&>& role) {
        if (!role) {
          return;
        }
        const Library::Language::Function& function = *role;
        if (!function.declares_self() ||
            function.get_parameters().get_size() != 1 ||
            !function.get_results().is_empty()) {
          cursor.create_expression_error(
              function.get_anchor(),
              "Scene lifecycle role requires `[self] -> []`."_view);
          valid = False;
        }
      };
  validate_empty(prepare);
  validate_empty(pause);
  validate_empty(resume);
  validate_empty(release);

  const Library::Language::Function& update_function = *update;
  auto delta = update_function.get_parameters().get_abstract(1);
  auto delta_addressable = delta ? delta->select<Ttx::Model::Addressable>()
                                 : Option<const Ttx::Model::Addressable&>();
  const Abstract& r64 = library.resolve_concept("R64"_view).resolve();
  if (!update_function.declares_self() ||
      update_function.get_parameters().get_size() != 2 ||
      !update_function.get_results().is_empty() || !delta_addressable ||
      delta_addressable->get_name() != "delta_time"_view ||
      &delta_addressable->get_type().resolve() != &r64) {
    cursor.create_expression_error(
        update_function.get_anchor(),
        "Scene update requires `[self, .delta_time : R64] -> []`."_view);
    valid = False;
  }
  return valid;
}

auto Scene::Language::Monograph::validate_lifecycle_restored() const -> Bool {
  auto prepare = get_lifecycle(Lifecycle::Prepare);
  auto pause = get_lifecycle(Lifecycle::Pause);
  auto resume = get_lifecycle(Lifecycle::Resume);
  auto update = get_lifecycle(Lifecycle::Update);
  auto release = get_lifecycle(Lifecycle::Release);
  BAIL_IF(!prepare || !update || !release);
  auto valid_empty =
      [](const Option<const Library::Language::Function&>& role) {
        return !role || (role->declares_self() &&
                         role->get_parameters().get_size() == 1 &&
                         role->get_results().is_empty());
      };
  BAIL_IF(
      !valid_empty(prepare) || !valid_empty(pause) || !valid_empty(resume) ||
      !valid_empty(release));

  const Library::Language::Function& function = *update;
  auto delta = function.get_parameters().get_abstract(1);
  auto addressable = delta ? delta->select<Ttx::Model::Addressable>()
                           : Option<const Ttx::Model::Addressable&>();
  const Abstract& r64 = library.resolve_concept("R64"_view).resolve();
  return function.declares_self() &&
         function.get_parameters().get_size() == 2 &&
         function.get_results().is_empty() && addressable &&
         addressable->get_name() == "delta_time"_view &&
         &addressable->get_type().resolve() == &r64;
}
