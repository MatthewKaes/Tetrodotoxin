// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/platform/wayland/input.hpp"

#include "validation/unit_test.hpp"

#include <linux/input-event-codes.h>

using namespace Perimortem;
using namespace Validation;

static Harness WaylandInput = {
  .name = "Platform::Wayland::Input"_view,
};

PERIMORTEM_UNIT_TEST(WaylandInput, key_translation) {
  EXPECT(
      Platform::Wayland::Input::translate_keyboard(KEY_A) ==
      System::Input::Key::A);
  EXPECT(
      Platform::Wayland::Input::translate_keyboard(KEY_F24) ==
      System::Input::Key::F24);
  EXPECT(
      Platform::Wayland::Input::translate_keyboard(KEY_RO) ==
      System::Input::Key::InternationalRo);
  EXPECT(
      Platform::Wayland::Input::translate_keyboard(KEY_RESERVED) ==
      System::Input::Key::None);
  EXPECT(
      Platform::Wayland::Input::translate_button(BTN_LEFT) ==
      System::Input::Key::MousePrimary);
  EXPECT(
      Platform::Wayland::Input::translate_button(BTN_BACK) ==
      System::Input::Key::MouseBack);
}
