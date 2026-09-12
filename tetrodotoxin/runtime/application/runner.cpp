// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/runtime/application/runner.hpp"

#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/null_terminated.hpp"
#include "perimortem/core/time.hpp"

#include "perimortem/memory/dynamic/vector.hpp"

#include "perimortem/system/input.hpp"

#include "perimortem/abi/system/input.hpp"
#include "perimortem/platform/window.hpp"
#include "perimortem/vulkan/pipelines.hpp"
#include "perimortem/vulkan/renderer.hpp"
#include "tetrodotoxin/graphics/runtime/compiled_children_2d.hpp"
#include "tetrodotoxin/graphics/runtime/submission.hpp"
#include "tetrodotoxin/runtime/application/session.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin;

auto Runtime::Application::Runner::run(const Product& product) -> int {
  if (product.title == nullptr || product.width == 0 || product.height == 0 ||
      product.initial_scene >= product.scene_count ||
      product.graphics_type_count == 0 ||
      product.graphics_placements == nullptr ||
      product.graphics_children == nullptr ||
      product.graphics_drawables == nullptr || product.programs == nullptr ||
      product.program_count == 0) {
    return 1;
  }

  Platform::Window window(
      product.width, product.height,
      reinterpret_cast<const char*>(product.title));
  Platform::Window::EventStatus initial_window_status =
      window.get_event_status();
  if (initial_window_status != Platform::Window::EventStatus::Ready) {
    if (initial_window_status == Platform::Window::EventStatus::Failed) {
      Core::Diagnostics::Log::error("Application window setup failed."_view);
      return 1;
    }
    return 0;
  }
  Memory::Dynamic::Vector<const Tetrodotoxin::Graphics::Runtime::Placement2D*>
      graphics_placements(product.graphics_type_count);
  Memory::Dynamic::Vector<const Tetrodotoxin::Graphics::Runtime::Children2D*>
      graphics_children(product.graphics_type_count);
  Memory::Dynamic::Vector<const Tetrodotoxin::Graphics::Runtime::Drawable2D*>
      graphics_drawables(product.graphics_type_count);
  for (Count index = 0; index < product.graphics_type_count; index++) {
    PlacementProvider placement_provider = product.graphics_placements[index];
    ChildrenProvider children_provider = product.graphics_children[index];
    DrawableProvider drawable_provider = product.graphics_drawables[index];
    const auto* placement = placement_provider ? placement_provider() : nullptr;
    const auto* children = children_provider ? children_provider() : nullptr;
    const auto* drawable = drawable_provider ? drawable_provider() : nullptr;
    if (placement == nullptr && children == nullptr && drawable == nullptr) {
      return 1;
    }
    graphics_placements.insert(placement);
    graphics_children.insert(children);
    graphics_drawables.insert(drawable);
  }
  U32 physical_width = window.get_logical_width() * window.get_scale();
  U32 physical_height = window.get_logical_height() * window.get_scale();
  Vulkan::Renderer renderer(
      window.get_presentation(), physical_width, physical_height);
  Vulkan::Pipelines pipelines(
      renderer.get_context(), renderer.get_swapchain().get_format(),
      Core::View::Vector<Vulkan::Description::Program>(
          product.programs, product.program_count));
  Runtime::Application::Session session(product);
  if (!session.start()) {
    return 1;
  }

  // The runtime keeps authored lifecycle ahead of graphics extraction. A Scene
  // can update its real Scene Objects first, then each Interface reads that
  // completed state for the frame without another retained Scene inventory.
  Core::Time previous = Core::Time::now();
  Bool successful = True;
  while (session.is_running()) {
    Platform::Window::EventStatus event_status = window.poll_events();
    if (event_status != Platform::Window::EventStatus::Ready) {
      if (event_status == Platform::Window::EventStatus::Failed) {
        Core::Diagnostics::Log::error(
            "Application window event polling failed."_view);
        successful = False;
      }
      break;
    }

    Abi::System::publish_input(window.get_input());
    if (window.get_input().is_pressed(System::Input::Key::Escape)) {
      session.stop();
      break;
    }

    Core::Time current = Core::Time::now();
    R64 delta = previous.measure(current).convert_to_seconds();
    previous = current;
    if (!session.update(delta)) {
      Core::Diagnostics::Log::error("Application Scene update failed."_view);
      successful = False;
      break;
    }

    if (window.get_needs_resize()) {
      physical_width = window.get_logical_width() * window.get_scale();
      physical_height = window.get_logical_height() * window.get_scale();
      if (physical_width != 0 && physical_height != 0 &&
          renderer.resize(physical_width, physical_height)) {
        pipelines.rebuild(renderer.get_swapchain().get_format());
      }
      window.clear_resize();
    }

    auto active_scene = session.get_active_scene();
    if (!active_scene) {
      Core::Diagnostics::Log::error("Application lost its active Scene."_view);
      successful = False;
      break;
    }
    Tetrodotoxin::Graphics::Runtime::CompiledChildren2D scene_children(
        active_scene->graphics_child_count, active_scene->graphics_children);
    auto submission = Tetrodotoxin::Graphics::Runtime::PassUI::create(
        session.get_active_object(), scene_children.get_children(),
        graphics_placements.get_view(), graphics_children.get_view(),
        graphics_drawables.get_view());
    if (!submission) {
      Core::Diagnostics::Log::error(
          "Application could not collect one frame submission."_view);
      successful = False;
      break;
    }
    Vulkan::Renderer::Frame frame;
    if (!renderer.begin_frame(frame)) {
      if (physical_width != 0 && physical_height != 0 &&
          renderer.resize(physical_width, physical_height)) {
        pipelines.rebuild(renderer.get_swapchain().get_format());
      }
      continue;
    }
    Bool frame_complete = pipelines.record(
        frame.command_buffer, frame.width, frame.height,
        submission->get_batches());
    renderer.end_frame(frame);
    if (!frame_complete) {
      Core::Diagnostics::Log::error(
          "Application could not record one graphics frame."_view);
      successful = False;
      session.stop();
      break;
    }
    session.commit();
  }

  session.stop();
  return successful ? 0 : 1;
}

extern "C" auto tetrodotoxin_application_scene(
    const Runtime::Application::Product* product) -> int {
  return product ? Runtime::Application::Runner::run(*product) : 1;
}
