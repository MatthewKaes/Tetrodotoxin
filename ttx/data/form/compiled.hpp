// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/static/vector.hpp"

#include "perimortem/memory/const/object.hpp"
#include "perimortem/memory/const/range.hpp"
#include "perimortem/memory/const/vector.hpp"

#include "ttx/data/form/compiler.hpp"

namespace Ttx::Data::Form {

// A benefit of the TTX abstraction design is that systems are decoupled enough
// that we can take advantage of modern language features while still providing
// the stable C ABI. `Compiled` gives C++ clients an easy way to compile their
// Schemas as build time squeeze out additional TTX bootstraping gains.
//
// Both sizing and construction use Compiler's normalization rules. The
// source and every reachable description must be readable during constant
// evaluation and the publication remains borrowed from its owning module.
template <const Schema& source>
class Compiled {
 private:
  using CompRep = Perimortem::Memory::Const::Object<Representation>;
  using CompElem = Perimortem::Memory::Const::Range<Representation::Element>;
  using CompSpan = Perimortem::Memory::Const::Range<Representation::Composite>;

  // One output policy handles both passes. Without final arrays it owns the
  // transient records used for sizing. With final arrays it places records
  // there, while its source memo still expires at the end of construction.
  template <
      typename publish_lambda,
      typename elements_lambda,
      typename composites_lambda>
  class Driver {
   public:
    // Store the lambda's as public functions to feed compilation.
    publish_lambda publish;
    elements_lambda publish_elements;
    composites_lambda publish_composites;

    // We need to forward the Const::Vector representation for positions so the
    // compiler performs its own computation in a constexpr way.
    using Elements = Perimortem::Memory::Const::Vector<Representation::Element>;
    using Composites =
        Perimortem::Memory::Const::Vector<Representation::Composite>;

    consteval Driver(
        publish_lambda&& publish,
        elements_lambda&& publish_elements,
        composites_lambda&& publish_composites)
        : publish(static_cast<publish_lambda&&>(publish)),
          publish_elements(static_cast<elements_lambda&&>(publish_elements)),
          publish_composites(
              static_cast<composites_lambda&&>(publish_composites)) {}

    consteval auto find(const Schema* schema)
        -> Perimortem::Core::Option<const Representation*> {
      for (Count i = 0; i < sources.get_size(); ++i) {
        if (sources[i].schema == schema) {
          return sources[i].result;
        }
      }

      return {};
    }

    consteval auto cache(const Schema* schema, const Representation* result)
        -> void {
      for (Count i = 0; i < sources.get_size(); ++i) {
        if (sources[i].schema == schema) {
          sources[i].result = result;
          return;
        }
      }

      sources.insert(Source(schema, result));
    }

    consteval auto forget(const Schema* schema) -> void {
      for (Count i = 0; i < sources.get_size(); ++i) {
        if (sources[i].schema == schema) {
          sources.remove(i);
          return;
        }
      }
    }

   private:
    struct Source {
      const Schema* schema;
      const Representation* result;
    };

    Perimortem::Memory::Const::Vector<Source> sources;
  };

  struct Layout {
    Count nodes;
    Count elements;
    Count composites;
  };

  // These private construction helpers belong to the static owner because
  // their answers determine its type and array sizes. Keeping them together
  // avoids exposing a separate sizing protocol or another set of normalization
  // rules.
  static consteval auto measure() -> Layout {
    Perimortem::Memory::Const::Vector<CompRep> representations;
    Perimortem::Memory::Const::Vector<CompElem> elements;
    Perimortem::Memory::Const::Vector<CompSpan> composites;
    Count element_entries = 0;
    Count composite_entries = 0;

    Driver driver(
        [&](const Representation& value) -> const Representation* {
          return representations.insert(value).get_data();
        },
        [&](Perimortem::Core::View::Vector<Representation::Element> entries)
            -> const Representation::Element* {
          if (!entries.get_size()) {
            return nullptr;
          }

          auto* result = elements.insert(entries.get_size()).get_data();
          for (Count i = 0; i < entries.get_size(); ++i) {
            result[i] = entries[i];
          }

          element_entries += entries.get_size();
          return result;
        },
        [&](Perimortem::Core::View::Vector<Representation::Composite> entries)
            -> const Representation::Composite* {
          if (!entries.get_size()) {
            return nullptr;
          }

          auto* result = composites.insert(entries.get_size()).get_data();
          for (Count i = 0; i < entries.get_size(); ++i) {
            result[i] = entries[i];
          }

          composite_entries += entries.get_size();
          return result;
        });

    // Perform the actual compilation on the source using our stub driver to
    // capture all of the resulting construction information and return it as
    // the required layout for our actual static storage version.
    const auto status = Compiler(driver).compile(&source);
    if (status != Status::Success) {
      return {};
    }

    return Layout(
        representations.get_size(), element_entries, composite_entries);
  }

  // We need to compile the Schema twice at compile time which is annoying but
  // isn't too expensive. This lets us do one dynamic compile to measure the
  // layout so we can then construct our actual storage.
  static constexpr auto layout = measure();
  // Even Empty needs one published record. Zero nodes therefore lets us reject
  // a failed measurement after its temporary driver has finished evaluation.
  static_assert(layout.nodes != 0, "Invalid constant Schema");

  struct Storage {
    // A spare private slot avoids a zero length C++ array for forms with no
    // position tables and is never published as a logical position.
    Perimortem::Core::Static::
        Vector<Representation::Element, layout.elements ? layout.elements : 1>
            elements;
    Perimortem::Core::Static::Vector<
        Representation::Composite,
        layout.composites ? layout.composites : 1>
        composites;
    Perimortem::Core::Static::Vector<Representation, layout.nodes>
        representations;
    const Representation* root = nullptr;

    consteval Storage() {
      Count node_count = 0;
      Count element_entries = 0;
      Count composite_entries = 0;
      Driver driver(
          [&](const Representation& value) -> const Representation* {
            representations[node_count] = value;
            return &representations[node_count++];
          },
          [&](Perimortem::Core::View::Vector<Representation::Element> entries)
              -> const Representation::Element* {
            auto* result = elements.get_data() + element_entries;
            for (Count i = 0; i < entries.get_size(); ++i) {
              result[i] = entries[i];
            }

            element_entries += entries.get_size();
            return entries.get_size() ? result : nullptr;
          },
          [&](Perimortem::Core::View::Vector<Representation::Composite> entries)
              -> const Representation::Composite* {
            auto* result = composites.get_data() + composite_entries;
            for (Count i = 0; i < entries.get_size(); ++i) {
              result[i] = entries[i];
            }

            composite_entries += entries.get_size();
            return entries.get_size() ? result : nullptr;
          });

      // Sizing has already established that this source compiles successfully.
      // Use its cached result because normalization can replace the root
      // with an existing child rather than publishing another record.
      Compiler(driver).compile(&source);
      root = *driver.find(&source);
    }
  };

 public:
  static constexpr auto get_representation() -> const Representation& {
    static constexpr Storage pre_compiled;
    return *pre_compiled.root;
  }
};

}  // namespace Ttx::Data::Form
