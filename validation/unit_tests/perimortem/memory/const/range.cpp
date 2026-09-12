// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/memory/const/range.hpp"
#include "perimortem/memory/const/object.hpp"

using namespace Perimortem::Memory;

// A const observation must use the same allocation and extent as mutable
// indexing, without making the owner itself mutable to read an element.
static_assert([] {
  Const::Range<int> source(2);
  source[1] = 42;
  const auto& view = source;
  return view.at(1) == 42 && view[1] == 42 && view.get_size() == 2;
}(), "Range provides both const and mutable indexed access");

static_assert([] {
  Const::Range<int> source(2);
  source.get_data()[0] = 42;
  Const::Range<int> copy(source);
  copy.get_data()[0] = 7;
  auto& self = source;
  source = self;
  source = static_cast<Const::Range<int>&&>(self);

  Const::Range<int> destination(8);
  destination = copy;
  return destination.get_size() == 2 && destination.get_data()[0] == 7 &&
         source.get_size() == 2 && source.get_data()[0] == 42;
}(), "Range copies and self assignment preserve the selected values");

// Position tables retain pointers into their Range. Moving it must preserve
// those pointers, and replacing it with an empty Range must release ownership.
static_assert([] {
  Const::Range<Const::Object<int>> source(2);
  source.get_data()[1] = Const::Object<int>(42);
  const auto* retained = source.get_data();
  Const::Range<Const::Object<int>> moved(
      static_cast<Const::Range<Const::Object<int>>&&>(source));
  Const::Range<Const::Object<int>> destination;
  destination = static_cast<Const::Range<Const::Object<int>>&&>(moved);
  const bool preserved = destination.get_data() == retained &&
                         *retained[1].get_data() == 42;
  destination = Const::Range<Const::Object<int>>();
  return preserved && !source.get_size() && !source.get_data() &&
         !moved.get_size() && !moved.get_data() && !destination.get_size();
}(), "Range moves preserve the array and empty assignment retires it");
