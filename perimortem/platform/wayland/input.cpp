// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

// Wayland and Linux input headers stay in the platform implementation so the
// host neutral Input snapshot remains usable by headless code and other hosts.
#include "perimortem/platform/wayland/input.hpp"

#include <linux/input-event-codes.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>

using namespace Perimortem::Core;
using namespace Perimortem;

// Wayland added release requests after the original object destruction path.
// Selecting from the proxy version keeps teardown valid for older seats while
// letting modern compositors observe the cooperative release.
static auto release(wl_keyboard* keyboard) -> void {
  if (wl_keyboard_get_version(keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION) {
    wl_keyboard_release(keyboard);
  } else {
    wl_keyboard_destroy(keyboard);
  }
}

static auto release(wl_pointer* pointer) -> void {
  if (wl_pointer_get_version(pointer) >= WL_POINTER_RELEASE_SINCE_VERSION) {
    wl_pointer_release(pointer);
  } else {
    wl_pointer_destroy(pointer);
  }
}

static auto release(wl_seat* seat) -> void {
  if (wl_seat_get_version(seat) >= WL_SEAT_RELEASE_SINCE_VERSION) {
    wl_seat_release(seat);
  } else {
    wl_seat_destroy(seat);
  }
}

Platform::Wayland::Input::~Input() {
  destroy();
}

auto Platform::Wayland::Input::attach(void* selected_surface) -> Bool {
  if (!selected_surface || surface) {
    return False;
  }
  surface = selected_surface;
  return True;
}

auto Platform::Wayland::Input::register_global(
    void* registry,
    U32 name,
    const char* interface,
    U32 version) -> void {
  if (seat || strcmp(interface, wl_seat_interface.name) != 0) {
    return;
  }

  U32 selected_version = version < 9 ? version : 9;
  seat = static_cast<wl_seat*>(wl_registry_bind(
      static_cast<wl_registry*>(registry), name, &wl_seat_interface,
      selected_version));
  if (seat) {
    static const wl_seat_listener listener = {
      [](void* data, wl_seat*, uint32_t capabilities) {
        on_seat_capabilities(data, U32(capabilities));
      },
      [](void*, wl_seat*, const char*) {},
    };
    seat_name = name;
    wl_seat_add_listener(static_cast<wl_seat*>(seat), &listener, this);
  }
}

auto Platform::Wayland::Input::remove_global(U32 name) -> void {
  if (seat_name == name) {
    release_devices();
  }
}

auto Platform::Wayland::Input::set_key(System::Input::Key key, Bool down)
    -> void {
  Count index = Count(key);
  if (key == System::Input::Key::None || index >= System::Input::key_count) {
    return;
  }

  U64 mask = U64(1) << (index % word_bits);
  if (down) {
    current[index / word_bits] |= mask;
  } else {
    current[index / word_bits] &= ~mask;
  }
}

auto Platform::Wayland::Input::clear_keyboard() -> void {
  for (Count index = Count(System::Input::Key::Escape);
       index < Count(System::Input::Key::MousePrimary); index++) {
    set_key(System::Input::Key(index), False);
  }
}

auto Platform::Wayland::Input::clear_pointer() -> void {
  for (Count index = Count(System::Input::Key::MousePrimary);
       index <= Count(System::Input::Key::MouseEight); index++) {
    set_key(System::Input::Key(index), False);
  }
  pointer_focused = False;
}

auto Platform::Wayland::Input::collect(const System::Input::Mapping& mapping)
    -> System::Input {
  Core::Static::Vector<System::Input::Key, System::Input::key_count>
      current_keys;
  Core::Static::Vector<System::Input::Key, System::Input::key_count>
      previous_keys;
  Count current_count = 0;
  Count previous_count = 0;
  for (Count index = 1; index < System::Input::key_count; index++) {
    U64 mask = U64(1) << (index % word_bits);
    if ((current[index / word_bits] & mask) != 0) {
      current_keys[current_count++] = System::Input::Key(index);
    }
    if ((previous[index / word_bits] & mask) != 0) {
      previous_keys[previous_count++] = System::Input::Key(index);
    }
  }

  System::Input::Pointer pointer_state = {
    pointer_x,
    pointer_y,
    pointer_focused && previous_pointer_focused ? pointer_x - previous_pointer_x
                                                : R32(0),
    pointer_focused && previous_pointer_focused ? pointer_y - previous_pointer_y
                                                : R32(0),
    scroll_x,
    scroll_y,
    pointer_focused,
  };
  System::Input output = System::Input::create(
      current_keys.slice(0, current_count),
      previous_keys.slice(0, previous_count), mapping, pointer_state);

  previous = current;
  previous_pointer_x = pointer_x;
  previous_pointer_y = pointer_y;
  previous_pointer_focused = pointer_focused;
  scroll_x = 0;
  scroll_y = 0;
  return output;
}

auto Platform::Wayland::Input::release_devices() -> void {
  if (keyboard) {
    release(static_cast<wl_keyboard*>(keyboard));
    keyboard = nullptr;
  }
  if (pointer) {
    release(static_cast<wl_pointer*>(pointer));
    pointer = nullptr;
  }
  if (seat) {
    release(static_cast<wl_seat*>(seat));
    seat = nullptr;
  }
  seat_name = 0;
  clear_keyboard();
  clear_pointer();
}

auto Platform::Wayland::Input::destroy() -> void {
  release_devices();
  surface = nullptr;
}

auto Platform::Wayland::Input::on_seat_capabilities(
    void* data,
    U32 capabilities) -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  Bool has_keyboard = Bool((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0);
  Bool has_pointer = Bool((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0);
  auto* native_seat = static_cast<wl_seat*>(input->seat);

  if (has_keyboard && !input->keyboard) {
    input->keyboard = wl_seat_get_keyboard(native_seat);
    if (input->keyboard) {
      static const wl_keyboard_listener listener = {
        [](void*, wl_keyboard*, uint32_t, int32_t descriptor, uint32_t) {
          on_keyboard_keymap(S32(descriptor));
        },
        [](void* context, wl_keyboard*, uint32_t, wl_surface* surface,
           wl_array* keys) {
          on_keyboard_enter(
              context, surface, static_cast<const U32*>(keys->data),
              keys->size / sizeof(U32));
        },
        [](void* context, wl_keyboard*, uint32_t, wl_surface* surface) {
          on_keyboard_leave(context, surface);
        },
        [](void* context, wl_keyboard*, uint32_t, uint32_t, uint32_t key,
           uint32_t state) { on_keyboard_key(context, U32(key), U32(state)); },
        [](void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t,
           uint32_t) {},
        [](void*, wl_keyboard*, int32_t, int32_t) {},
      };
      wl_keyboard_add_listener(
          static_cast<wl_keyboard*>(input->keyboard), &listener, input);
    }
  } else if (!has_keyboard && input->keyboard) {
    release(static_cast<wl_keyboard*>(input->keyboard));
    input->keyboard = nullptr;
    input->keyboard_focused = False;
    input->clear_keyboard();
  }

  if (has_pointer && !input->pointer) {
    input->pointer = wl_seat_get_pointer(native_seat);
    if (input->pointer) {
      static const wl_pointer_listener listener = {
        [](void* context, wl_pointer*, uint32_t, wl_surface* surface,
           wl_fixed_t x, wl_fixed_t y) {
          on_pointer_enter(context, surface, S32(x), S32(y));
        },
        [](void* context, wl_pointer*, uint32_t, wl_surface* surface) {
          on_pointer_leave(context, surface);
        },
        [](void* context, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
          on_pointer_motion(context, S32(x), S32(y));
        },
        [](void* context, wl_pointer*, uint32_t, uint32_t, uint32_t button,
           uint32_t state) {
          on_pointer_button(context, U32(button), U32(state));
        },
        [](void* context, wl_pointer*, uint32_t, uint32_t axis,
           wl_fixed_t value) {
          on_pointer_axis(context, U32(axis), S32(value));
        },
        [](void*, wl_pointer*) {},
        [](void*, wl_pointer*, uint32_t) {},
        [](void*, wl_pointer*, uint32_t, uint32_t) {},
        [](void*, wl_pointer*, uint32_t, int32_t) {},
        [](void*, wl_pointer*, uint32_t, int32_t) {},
        [](void*, wl_pointer*, uint32_t, uint32_t) {},
      };
      wl_pointer_add_listener(
          static_cast<wl_pointer*>(input->pointer), &listener, input);
    }
  } else if (!has_pointer && input->pointer) {
    release(static_cast<wl_pointer*>(input->pointer));
    input->pointer = nullptr;
    input->clear_pointer();
  }
}

auto Platform::Wayland::Input::on_keyboard_keymap(S32 descriptor) -> void {
  if (descriptor >= 0) {
    close(descriptor);
  }
}

auto Platform::Wayland::Input::on_keyboard_enter(
    void* data,
    void* entered,
    const U32* keys,
    Count key_count) -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (entered != input->surface) {
    return;
  }

  input->clear_keyboard();
  input->keyboard_focused = True;
  for (Count index = 0; index < key_count; index++) {
    input->set_key(translate_keyboard(keys[index]), True);
  }
}

auto Platform::Wayland::Input::on_keyboard_leave(void* data, void* left)
    -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (left == input->surface) {
    input->keyboard_focused = False;
    input->clear_keyboard();
  }
}

auto Platform::Wayland::Input::on_keyboard_key(void* data, U32 key, U32 state)
    -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (input->keyboard_focused) {
    input->set_key(
        translate_keyboard(key), Bool(state != WL_KEYBOARD_KEY_STATE_RELEASED));
  }
}

auto Platform::Wayland::Input::on_pointer_enter(
    void* data,
    void* entered,
    S32 x,
    S32 y) -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (entered != input->surface) {
    return;
  }

  input->pointer_x = R32(wl_fixed_to_double(x));
  input->pointer_y = R32(wl_fixed_to_double(y));
  input->previous_pointer_x = input->pointer_x;
  input->previous_pointer_y = input->pointer_y;
  input->pointer_focused = True;
  input->previous_pointer_focused = False;
}

auto Platform::Wayland::Input::on_pointer_leave(void* data, void* left)
    -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (left == input->surface) {
    input->clear_pointer();
  }
}

auto Platform::Wayland::Input::on_pointer_motion(void* data, S32 x, S32 y)
    -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (input->pointer_focused) {
    input->pointer_x = R32(wl_fixed_to_double(x));
    input->pointer_y = R32(wl_fixed_to_double(y));
  }
}

auto Platform::Wayland::Input::on_pointer_button(
    void* data,
    U32 button,
    U32 state) -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (input->pointer_focused) {
    input->set_key(
        translate_button(button),
        Bool(state == WL_POINTER_BUTTON_STATE_PRESSED));
  }
}

auto Platform::Wayland::Input::on_pointer_axis(void* data, U32 axis, S32 value)
    -> void {
  auto* input = static_cast<Platform::Wayland::Input*>(data);
  if (!input->pointer_focused) {
    return;
  }

  R32 amount = R32(wl_fixed_to_double(value));
  if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    input->scroll_x += amount;
  } else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    input->scroll_y += amount;
  }
}

auto Platform::Wayland::Input::translate_keyboard(U32 code)
    -> System::Input::Key {
  switch (code) {
  case KEY_ESC:
    return System::Input::Key::Escape;
  case KEY_F1:
    return System::Input::Key::F1;
  case KEY_F2:
    return System::Input::Key::F2;
  case KEY_F3:
    return System::Input::Key::F3;
  case KEY_F4:
    return System::Input::Key::F4;
  case KEY_F5:
    return System::Input::Key::F5;
  case KEY_F6:
    return System::Input::Key::F6;
  case KEY_F7:
    return System::Input::Key::F7;
  case KEY_F8:
    return System::Input::Key::F8;
  case KEY_F9:
    return System::Input::Key::F9;
  case KEY_F10:
    return System::Input::Key::F10;
  case KEY_F11:
    return System::Input::Key::F11;
  case KEY_F12:
    return System::Input::Key::F12;
  case KEY_F13:
    return System::Input::Key::F13;
  case KEY_F14:
    return System::Input::Key::F14;
  case KEY_F15:
    return System::Input::Key::F15;
  case KEY_F16:
    return System::Input::Key::F16;
  case KEY_F17:
    return System::Input::Key::F17;
  case KEY_F18:
    return System::Input::Key::F18;
  case KEY_F19:
    return System::Input::Key::F19;
  case KEY_F20:
    return System::Input::Key::F20;
  case KEY_F21:
    return System::Input::Key::F21;
  case KEY_F22:
    return System::Input::Key::F22;
  case KEY_F23:
    return System::Input::Key::F23;
  case KEY_F24:
    return System::Input::Key::F24;
  case KEY_SYSRQ:
    return System::Input::Key::PrintScreen;
  case KEY_SCROLLLOCK:
    return System::Input::Key::ScrollLock;
  case KEY_PAUSE:
    return System::Input::Key::Pause;
  case KEY_GRAVE:
    return System::Input::Key::Grave;
  case KEY_1:
    return System::Input::Key::Digit1;
  case KEY_2:
    return System::Input::Key::Digit2;
  case KEY_3:
    return System::Input::Key::Digit3;
  case KEY_4:
    return System::Input::Key::Digit4;
  case KEY_5:
    return System::Input::Key::Digit5;
  case KEY_6:
    return System::Input::Key::Digit6;
  case KEY_7:
    return System::Input::Key::Digit7;
  case KEY_8:
    return System::Input::Key::Digit8;
  case KEY_9:
    return System::Input::Key::Digit9;
  case KEY_0:
    return System::Input::Key::Digit0;
  case KEY_MINUS:
    return System::Input::Key::Minus;
  case KEY_EQUAL:
    return System::Input::Key::Equal;
  case KEY_BACKSPACE:
    return System::Input::Key::Backspace;
  case KEY_TAB:
    return System::Input::Key::Tab;
  case KEY_Q:
    return System::Input::Key::Q;
  case KEY_W:
    return System::Input::Key::W;
  case KEY_E:
    return System::Input::Key::E;
  case KEY_R:
    return System::Input::Key::R;
  case KEY_T:
    return System::Input::Key::T;
  case KEY_Y:
    return System::Input::Key::Y;
  case KEY_U:
    return System::Input::Key::U;
  case KEY_I:
    return System::Input::Key::I;
  case KEY_O:
    return System::Input::Key::O;
  case KEY_P:
    return System::Input::Key::P;
  case KEY_LEFTBRACE:
    return System::Input::Key::LeftBracket;
  case KEY_RIGHTBRACE:
    return System::Input::Key::RightBracket;
  case KEY_BACKSLASH:
    return System::Input::Key::Backslash;
  case KEY_CAPSLOCK:
    return System::Input::Key::CapsLock;
  case KEY_A:
    return System::Input::Key::A;
  case KEY_S:
    return System::Input::Key::S;
  case KEY_D:
    return System::Input::Key::D;
  case KEY_F:
    return System::Input::Key::F;
  case KEY_G:
    return System::Input::Key::G;
  case KEY_H:
    return System::Input::Key::H;
  case KEY_J:
    return System::Input::Key::J;
  case KEY_K:
    return System::Input::Key::K;
  case KEY_L:
    return System::Input::Key::L;
  case KEY_SEMICOLON:
    return System::Input::Key::Semicolon;
  case KEY_APOSTROPHE:
    return System::Input::Key::Apostrophe;
  case KEY_ENTER:
    return System::Input::Key::Enter;
  case KEY_LEFTSHIFT:
    return System::Input::Key::LeftShift;
  case KEY_Z:
    return System::Input::Key::Z;
  case KEY_X:
    return System::Input::Key::X;
  case KEY_C:
    return System::Input::Key::C;
  case KEY_V:
    return System::Input::Key::V;
  case KEY_B:
    return System::Input::Key::B;
  case KEY_N:
    return System::Input::Key::N;
  case KEY_M:
    return System::Input::Key::M;
  case KEY_COMMA:
    return System::Input::Key::Comma;
  case KEY_DOT:
    return System::Input::Key::Period;
  case KEY_SLASH:
    return System::Input::Key::Slash;
  case KEY_RIGHTSHIFT:
    return System::Input::Key::RightShift;
  case KEY_LEFTCTRL:
    return System::Input::Key::LeftControl;
  case KEY_LEFTMETA:
    return System::Input::Key::LeftSuper;
  case KEY_LEFTALT:
    return System::Input::Key::LeftAlt;
  case KEY_SPACE:
    return System::Input::Key::Space;
  case KEY_RIGHTALT:
    return System::Input::Key::RightAlt;
  case KEY_RIGHTMETA:
    return System::Input::Key::RightSuper;
  case KEY_MENU:
    return System::Input::Key::Menu;
  case KEY_RIGHTCTRL:
    return System::Input::Key::RightControl;
  case KEY_INSERT:
    return System::Input::Key::Insert;
  case KEY_DELETE:
    return System::Input::Key::Delete;
  case KEY_HOME:
    return System::Input::Key::Home;
  case KEY_END:
    return System::Input::Key::End;
  case KEY_PAGEUP:
    return System::Input::Key::PageUp;
  case KEY_PAGEDOWN:
    return System::Input::Key::PageDown;
  case KEY_UP:
    return System::Input::Key::ArrowUp;
  case KEY_DOWN:
    return System::Input::Key::ArrowDown;
  case KEY_LEFT:
    return System::Input::Key::ArrowLeft;
  case KEY_RIGHT:
    return System::Input::Key::ArrowRight;
  case KEY_NUMLOCK:
    return System::Input::Key::NumLock;
  case KEY_KPSLASH:
    return System::Input::Key::NumpadDivide;
  case KEY_KPASTERISK:
    return System::Input::Key::NumpadMultiply;
  case KEY_KPMINUS:
    return System::Input::Key::NumpadSubtract;
  case KEY_KPPLUS:
    return System::Input::Key::NumpadAdd;
  case KEY_KPENTER:
    return System::Input::Key::NumpadEnter;
  case KEY_KP0:
    return System::Input::Key::Numpad0;
  case KEY_KP1:
    return System::Input::Key::Numpad1;
  case KEY_KP2:
    return System::Input::Key::Numpad2;
  case KEY_KP3:
    return System::Input::Key::Numpad3;
  case KEY_KP4:
    return System::Input::Key::Numpad4;
  case KEY_KP5:
    return System::Input::Key::Numpad5;
  case KEY_KP6:
    return System::Input::Key::Numpad6;
  case KEY_KP7:
    return System::Input::Key::Numpad7;
  case KEY_KP8:
    return System::Input::Key::Numpad8;
  case KEY_KP9:
    return System::Input::Key::Numpad9;
  case KEY_KPDOT:
    return System::Input::Key::NumpadDecimal;
  case KEY_KPEQUAL:
    return System::Input::Key::NumpadEqual;
  case KEY_KPCOMMA:
    return System::Input::Key::NumpadComma;
  case KEY_102ND:
    return System::Input::Key::NonUsBackslash;
  case KEY_RO:
    return System::Input::Key::InternationalRo;
  case KEY_YEN:
    return System::Input::Key::InternationalYen;
  case KEY_KATAKANAHIRAGANA:
    return System::Input::Key::Kana;
  case KEY_HENKAN:
    return System::Input::Key::Convert;
  case KEY_MUHENKAN:
    return System::Input::Key::NonConvert;
  case KEY_MUTE:
    return System::Input::Key::Mute;
  case KEY_VOLUMEDOWN:
    return System::Input::Key::VolumeDown;
  case KEY_VOLUMEUP:
    return System::Input::Key::VolumeUp;
  case KEY_PLAYPAUSE:
    return System::Input::Key::MediaPlayPause;
  case KEY_STOPCD:
    return System::Input::Key::MediaStop;
  case KEY_PREVIOUSSONG:
    return System::Input::Key::MediaPrevious;
  case KEY_NEXTSONG:
    return System::Input::Key::MediaNext;
  case KEY_BACK:
    return System::Input::Key::BrowserBack;
  case KEY_FORWARD:
    return System::Input::Key::BrowserForward;
  case KEY_REFRESH:
    return System::Input::Key::BrowserRefresh;
  case KEY_HOMEPAGE:
    return System::Input::Key::BrowserHome;
  default:
    return System::Input::Key::None;
  }
}

auto Platform::Wayland::Input::translate_button(U32 code)
    -> System::Input::Key {
  switch (code) {
  case BTN_LEFT:
    return System::Input::Key::MousePrimary;
  case BTN_RIGHT:
    return System::Input::Key::MouseSecondary;
  case BTN_MIDDLE:
    return System::Input::Key::MouseMiddle;
  case BTN_SIDE:
  case BTN_BACK:
    return System::Input::Key::MouseBack;
  case BTN_EXTRA:
  case BTN_FORWARD:
    return System::Input::Key::MouseForward;
  case BTN_TASK:
    return System::Input::Key::MouseSix;
  case BTN_0:
    return System::Input::Key::MouseSeven;
  case BTN_1:
    return System::Input::Key::MouseEight;
  default:
    return System::Input::Key::None;
  }
}
