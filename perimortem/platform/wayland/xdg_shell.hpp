// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/perimortem.hpp"

namespace Perimortem::Platform::Wayland {

// Owns the XDG protocol proxies that turn one Wayland surface into a toplevel.
// Protocol objects are generic Wayland proxies at the ABI boundary. Keeping
// those handles here avoids manufacturing incomplete C++ object types for each
// XML protocol name.
class XdgShell {
 public:
  struct Events {
    void* context = nullptr;
    void (*configure)(void*, S32, S32) = nullptr;
    void (*close)(void*) = nullptr;
  };

  XdgShell() = default;
  ~XdgShell();
  XdgShell(XdgShell&&) = delete;
  auto operator=(XdgShell&&) -> XdgShell& = delete;
  XdgShell(const XdgShell&) = delete;
  auto operator=(const XdgShell&) -> XdgShell& = delete;

  static auto recognizes(const char* interface) -> Bool;

  auto bind(void* registry, U32 name) -> Bool;
  auto create_toplevel(
      void* wayland_surface,
      const char* title,
      const char* application_id,
      Events events) -> Bool;
  auto destroy() -> void;

 private:
  using ListenerFunction = void (*)(void);

  static auto on_ping(void* data, void* shell, U32 serial) -> void;
  static auto on_surface_configure(void* data, void* shell, U32 serial) -> void;
  static auto
      on_toplevel_configure(void* data, void*, S32 width, S32 height, void*)
          -> void;
  static auto on_toplevel_close(void* data, void*) -> void;
  static auto on_toplevel_configure_bounds(void*, void*, S32, S32) -> void;
  static auto on_toplevel_wm_capabilities(void*, void*, void*) -> void;

  Events events;
  void* wm_base = nullptr;
  void* surface = nullptr;
  void* toplevel = nullptr;
  void* wayland_surface = nullptr;
};

}  // namespace Perimortem::Platform::Wayland
