// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/system/input.hpp"

#include "validation/unit_test.hpp"

#include "perimortem/core/static/vector.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::System;
using namespace Validation;

static Harness SystemInput = {
  .name = "System::Input"_view,
};

PERIMORTEM_UNIT_TEST(SystemInput, frame_transition) {
  Static::Vector<Input::Key, 2> previous = {{
    Input::Key::Shift,
    Input::Key::Shift,
  }};
  Static::Vector<Input::Key, 2> current = {{
    Input::Key::Space,
    Input::Key::Space,
  }};

  Input input = Input::create(current, previous);

  EXPECT(input.is_current(Input::Key::Space));
  EXPECT_NOT(input.is_current(Input::Key::Shift));
  EXPECT(input.is_pressed(Input::Key::Space));
  EXPECT_NOT(input.is_pressed(Input::Key::Shift));
  EXPECT_NOT(input.is_released(Input::Key::Space));
  EXPECT(input.is_released(Input::Key::Shift));
}

PERIMORTEM_UNIT_TEST(SystemInput, stable_snapshot) {
  Static::Vector<Input::Key, 2> held = {{
    Input::Key::Space,
    Input::Key::Shift,
  }};
  Input input = Input::create(held, held);

  EXPECT(input.is_current(Input::Key::Space));
  EXPECT(input.is_current(Input::Key::Shift));
  EXPECT_NOT(input.is_pressed(Input::Key::Space));
  EXPECT_NOT(input.is_pressed(Input::Key::Shift));
  EXPECT_NOT(input.is_released(Input::Key::Space));
  EXPECT_NOT(input.is_released(Input::Key::Shift));
  EXPECT_NOT(input.is_current(Input::Key::Count));
}

PERIMORTEM_UNIT_TEST(SystemInput, remapping) {
  Static::Vector<Input::Key, 2> previous = {{
    Input::Key::W,
    Input::Key::LeftShift,
  }};
  Static::Vector<Input::Key, 2> current = {{
    Input::Key::I,
    Input::Key::Space,
  }};
  Input::Mapping mapping;
  ASSERT(mapping.remap(Input::Key::W, Input::Key::ArrowUp));
  ASSERT(mapping.remap(Input::Key::I, Input::Key::ArrowUp));
  ASSERT(mapping.remap(Input::Key::LeftShift, Input::Key::A));
  ASSERT(mapping.unmap(Input::Key::Space));

  Input input = Input::create(current, previous, mapping);

  EXPECT(input.is_current(Input::Key::ArrowUp));
  EXPECT_NOT(input.is_pressed(Input::Key::ArrowUp));
  EXPECT_NOT(input.is_released(Input::Key::ArrowUp));
  EXPECT_NOT(input.is_current(Input::Key::Space));
  EXPECT(input.is_released(Input::Key::A));
  EXPECT_NOT(input.is_current(Input::Key::Shift));
  EXPECT(mapping.reset(Input::Key::Space));
  EXPECT(mapping.resolve(Input::Key::Space) == Input::Key::Space);
  EXPECT_NOT(mapping.remap(Input::Key::None, Input::Key::A));
  EXPECT_NOT(mapping.remap(Input::Key::A, Input::Key::Count));
}

PERIMORTEM_UNIT_TEST(SystemInput, pointer_snapshot) {
  Input::Pointer pointer = {
    120.5F, 64.25F, 2.0F, -4.0F, 1.5F, -3.0F, True,
  };
  Input input = Input::create({}, {}, pointer);

  EXPECT_EQ(input.get_pointer().x, R32(120.5));
  EXPECT_EQ(input.get_pointer().y, R32(64.25));
  EXPECT_EQ(input.get_pointer().delta_x, R32(2));
  EXPECT_EQ(input.get_pointer().delta_y, R32(-4));
  EXPECT_EQ(input.get_pointer().scroll_x, R32(1.5));
  EXPECT_EQ(input.get_pointer().scroll_y, R32(-3));
  EXPECT(input.get_pointer().active);
  EXPECT(sizeof(Input) <= 80);
}
