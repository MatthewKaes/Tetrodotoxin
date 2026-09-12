// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/platform/window.hpp"

#include "validation/unit_test.hpp"

using namespace Perimortem;

static Validation::Harness PlatformWindow = {
  .name = "Perimortem::Platform::Window"_view,
};

PERIMORTEM_UNIT_TEST(PlatformWindow, empty_window_fails) {
  Platform::Window window;
  EXPECT(window.get_event_status() == Platform::Window::EventStatus::Failed);
  EXPECT(window.poll_events() == Platform::Window::EventStatus::Failed);
}
