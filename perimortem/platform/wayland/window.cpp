// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

// Wayland stays in the platform implementation so Window and
// Presentation do not inherit its source or ABI vocabulary.
#include "perimortem/platform/window.hpp"

#if __has_include(<wayland-client.h>)
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <wayland-client.h>
#else
#error Wayland client headers are required by the Wayland Window backend
#endif

#include "perimortem/platform/wayland/xdg_shell.hpp"

using namespace Perimortem;
using namespace Perimortem::Platform;

static auto as_display(void* handle) -> wl_display* {
  return static_cast<wl_display*>(handle);
}

static auto as_registry(void* handle) -> wl_registry* {
  return static_cast<wl_registry*>(handle);
}

static auto as_compositor(void* handle) -> wl_compositor* {
  return static_cast<wl_compositor*>(handle);
}

static auto as_surface(void* handle) -> wl_surface* {
  return static_cast<wl_surface*>(handle);
}

Window::Window(U32 width, U32 height, const char* title) {
  initial_width = width;
  initial_height = height;
  logical_width = width;
  logical_height = height;

  display = wl_display_connect(nullptr);
  if (!display) {
    failed = True;
    return;
  }
  registry = wl_display_get_registry(as_display(display));
  if (!registry) {
    failed = True;
    return;
  }
  static const wl_registry_listener registry_listener = {
    [](void* data, wl_registry* registry, uint32_t name, const char* interface,
       uint32_t version) {
      on_registry_global(data, registry, U32(name), interface, U32(version));
    },
    [](void* data, wl_registry*, uint32_t name) {
      on_registry_global_remove(data, U32(name));
    },
  };
  if (wl_registry_add_listener(
          as_registry(registry), &registry_listener, this) != 0 ||
      wl_display_roundtrip(as_display(display)) < 0 || failed || !compositor) {
    failed = True;
    return;
  }

  surface = wl_compositor_create_surface(as_compositor(compositor));
  if (!surface) {
    failed = True;
    return;
  }
  static const wl_surface_listener surface_listener = {
    [](void*, wl_surface*, wl_output*) {},
    [](void*, wl_surface*, wl_output*) {},
    [](void* data, wl_surface*, int32_t factor) {
      on_surface_scale(data, S32(factor));
    },
    [](void*, wl_surface*, uint32_t) {},
  };
  if (wl_surface_add_listener(as_surface(surface), &surface_listener, this) !=
      0) {
    failed = True;
    return;
  }

  if (!shell.create_toplevel(
          surface, title, "perimortem",
          {
            this,
            &Window::on_shell_configure,
            &Window::on_shell_close,
          })) {
    failed = True;
    return;
  }

  if (!input_collector.attach(surface)) {
    failed = True;
    return;
  }

  wl_surface_commit(as_surface(surface));
  if (wl_display_roundtrip(as_display(display)) < 0) {
    failed = True;
  }
}

Window::~Window() {
  destroy();
}

auto Window::poll_events() -> Window::EventStatus {
  EventStatus status = get_event_status();
  if (status != EventStatus::Ready) {
    return status;
  }

  wl_display* native_display = as_display(display);
  if (wl_display_dispatch_pending(native_display) < 0) {
    failed = True;
    return EventStatus::Failed;
  }

  if (close_requested) {
    return EventStatus::Closed;
  }

  while (wl_display_prepare_read(native_display) != 0) {
    if (wl_display_dispatch_pending(native_display) < 0) {
      failed = True;
      return EventStatus::Failed;
    }

    if (close_requested) {
      return EventStatus::Closed;
    }
  }

  if (wl_display_flush(native_display) < 0) {
    wl_display_cancel_read(native_display);
    failed = True;
    return EventStatus::Failed;
  }

  pollfd display_fd = {
    wl_display_get_fd(native_display),
    POLLIN,
    0,
  };

  auto poll_result = 0;
  do {
    poll_result = poll(&display_fd, 1, 0);
  } while (poll_result < 0 && errno == EINTR);
  if (poll_result < 0) {
    wl_display_cancel_read(native_display);
    failed = True;
    return EventStatus::Failed;
  }

  if (poll_result > 0 && (display_fd.revents & POLLIN) != 0) {
    int events_read = wl_display_read_events(native_display);
    if (events_read < 0) {
      failed = True;
      return EventStatus::Failed;
    }
  } else {
    wl_display_cancel_read(native_display);
  }

  int events_dispatched = wl_display_dispatch_pending(native_display);
  if (events_dispatched < 0) {
    failed = True;
    return EventStatus::Failed;
  }

  input_snapshot = input_collector.collect(input_mapping);
  return close_requested ? EventStatus::Closed : EventStatus::Ready;
}

auto Window::get_event_status() const -> Window::EventStatus {
  if (failed || !display) {
    return EventStatus::Failed;
  }
  return close_requested ? EventStatus::Closed : EventStatus::Ready;
}

auto Window::get_logical_width() const -> U32 {
  return logical_width;
}

auto Window::get_logical_height() const -> U32 {
  return logical_height;
}

auto Window::get_scale() const -> U32 {
  return scale;
}

auto Window::get_needs_resize() const -> Bool {
  return needs_resize;
}

auto Window::clear_resize() -> void {
  needs_resize = False;
}

auto Window::get_input() const -> const System::Input& {
  return input_snapshot;
}

auto Window::get_input_mapping() -> System::Input::Mapping& {
  return input_mapping;
}

auto Window::get_input_mapping() const -> const System::Input::Mapping& {
  return input_mapping;
}

auto Window::get_presentation() const -> System::Presentation {
  return System::Presentation::create(
      System::Presentation::Kind::Wayland, display, surface);
}

auto Window::destroy() -> void {
  input_collector.destroy();
  shell.destroy();

  if (surface) {
    wl_surface_destroy(as_surface(surface));
    surface = nullptr;
  }

  if (compositor) {
    wl_compositor_destroy(as_compositor(compositor));
    compositor = nullptr;
  }

  if (registry) {
    wl_registry_destroy(as_registry(registry));
    registry = nullptr;
  }

  if (display) {
    wl_display_disconnect(as_display(display));
    display = nullptr;
  }
}

auto Window::on_shell_configure(void* data, S32 width, S32 height) -> void {
  auto* window = static_cast<Window*>(data);
  auto new_width = width > 0 ? static_cast<U32>(width) : window->initial_width;
  auto new_height =
      height > 0 ? static_cast<U32>(height) : window->initial_height;
  if (new_width != window->logical_width ||
      new_height != window->logical_height) {
    window->logical_width = new_width;
    window->logical_height = new_height;
    window->needs_resize = True;
  }
}

auto Window::on_shell_close(void* data) -> void {
  static_cast<Window*>(data)->close_requested = True;
}

auto Window::on_surface_scale(void* data, S32 factor) -> void {
  auto* window = static_cast<Window*>(data);
  if (static_cast<U32>(factor) != window->scale) {
    window->scale = static_cast<U32>(factor);
    window->needs_resize = True;
  }
}

auto Window::on_registry_global(
    void* data,
    void* registry,
    U32 name,
    const char* interface,
    U32 version) -> void {
  auto* window = static_cast<Window*>(data);
  window->input_collector.register_global(registry, name, interface, version);
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    window->compositor = static_cast<wl_compositor*>(wl_registry_bind(
        as_registry(registry), name, &wl_compositor_interface, 6));
  } else if (
      Platform::Wayland::XdgShell::recognizes(interface) &&
      !window->shell.bind(registry, name)) {
    window->failed = True;
  }
}

auto Window::on_registry_global_remove(void* data, U32 name) -> void {
  static_cast<Window*>(data)->input_collector.remove_global(name);
}
