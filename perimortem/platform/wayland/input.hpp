// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/static/vector.hpp"

#include "perimortem/system/input.hpp"

namespace Perimortem::Platform::Wayland {

// Input owns the Wayland seat objects that feed one Window. Native callbacks
// retain only physical state and pointer motion. collect() then publishes one
// immutable System snapshot after applying the caller's virtual key mapping.
class Input {
 public:
  Input() = default;
  ~Input();
  Input(Input&&) = delete;
  auto operator=(Input&&) -> Input& = delete;
  Input(const Input&) = delete;
  auto operator=(const Input&) -> Input& = delete;

  auto attach(void* surface) -> Bool;
  auto register_global(
      void* registry,
      U32 name,
      const char* interface,
      U32 version) -> void;
  auto remove_global(U32 name) -> void;
  auto collect(const System::Input::Mapping& mapping) -> System::Input;
  auto destroy() -> void;

  static auto translate_keyboard(U32 code) -> System::Input::Key;
  static auto translate_button(U32 code) -> System::Input::Key;

 private:
  static constexpr Count word_bits = sizeof(U64) * 8;
  static constexpr Count word_count =
      (System::Input::key_count + word_bits - 1) / word_bits;
  using KeyBits = Core::Static::Vector<U64, word_count>;

  auto set_key(System::Input::Key key, Bool down) -> void;
  auto clear_keyboard() -> void;
  auto clear_pointer() -> void;
  auto release_devices() -> void;

  static auto on_seat_capabilities(void* data, U32 capabilities) -> void;
  static auto on_keyboard_keymap(S32 descriptor) -> void;
  static auto on_keyboard_enter(
      void* data,
      void* surface,
      const U32* keys,
      Count key_count) -> void;
  static auto on_keyboard_leave(void* data, void* surface) -> void;
  static auto on_keyboard_key(void* data, U32 key, U32 state) -> void;

  static auto on_pointer_enter(void* data, void* surface, S32 x, S32 y) -> void;
  static auto on_pointer_leave(void* data, void* surface) -> void;
  static auto on_pointer_motion(void* data, S32 x, S32 y) -> void;
  static auto on_pointer_button(void* data, U32 button, U32 state) -> void;
  static auto on_pointer_axis(void* data, U32 axis, S32 value) -> void;

  void* seat = nullptr;
  void* keyboard = nullptr;
  void* pointer = nullptr;
  void* surface = nullptr;
  U32 seat_name = 0;
  KeyBits current;
  KeyBits previous;
  R32 pointer_x = 0;
  R32 pointer_y = 0;
  R32 previous_pointer_x = 0;
  R32 previous_pointer_y = 0;
  R32 scroll_x = 0;
  R32 scroll_y = 0;
  Bool keyboard_focused = False;
  Bool pointer_focused = False;
  Bool previous_pointer_focused = False;
};

}  // namespace Perimortem::Platform::Wayland
