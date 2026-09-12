// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/semantic/flow.hpp"

#include "ttx/semantic/block.hpp"
#include "ttx/semantic/direct.hpp"
#include "ttx/semantic/fragment.hpp"
#include "ttx/semantic/shared.hpp"

using namespace Ttx::Semantic;
using Ttx::Data::Form::Representation;

static auto reader_representation(const void* source)
    -> const ttx_representation* {
  return static_cast<const ttx_representation*>(source);
}

auto ttx_flow_reader(const ttx_representation* required) -> ttx_semantic_query {
  // Each role borrows the same required representation. Block also needs to
  // expose the destination surface supplied by an individual operation, so one
  // reader can serve several independently owned results through the same Flow.
  static const Direct::View::Operations direct = {reader_representation};
  static const Shared::View::Operations shared = {reader_representation};
  static const Fragment::View::Operations fragment = {reader_representation};
  static const Block::View::Operations block = {
    reader_representation,
    [](const void*, ttx_storage target) -> ttx_block_surface {
      return {target.data, target.representation->get_extent()};
    },
  };

  return {
    required,
    [](const void* source, perimortem_uuid requested,
       ttx_binding* result) -> ttx_binding_status {
      // This policy accepts each protocol explicitly. The writer still has to
      // supply its own matching role before Flow can establish cooperation.
      const Perimortem::System::Uuid id(requested);
      if (id == Direct::View::contract_id) {
        *result = Binding::provide<Direct::View>(source, direct).get_abi();
      } else if (id == Shared::View::contract_id) {
        *result = Binding::provide<Shared::View>(source, shared).get_abi();
      } else if (id == Block::View::contract_id) {
        *result = Binding::provide<Block::View>(source, block).get_abi();
      } else if (id == Fragment::View::contract_id) {
        *result = Binding::provide<Fragment::View>(source, fragment).get_abi();
      } else {
        return TTX_BINDING_UNSUPPORTED;
      }

      return TTX_BINDING_SATISFIED;
    }};
}

static auto failure(Binding::Failure status) -> Flow::Status {
  switch (status) {
  case Binding::Failure::Unsupported:
    return Flow::Status::Unsupported;
  case Binding::Failure::Pending:
    return Flow::Status::BindingPending;
  default:
    return Flow::Status::Rejected;
  }
}

// A candidate needs both bindings and an agreed payload ABI. Unsupported or
// incompatible candidates let us try another independent protocol, while a
// pending or rejected binding preserves the owner's decision. Comparing the
// ABI here keeps data access and lifetime acquisition after that agreement.
template <typename Protocol, typename Install>
static auto cooperate(Query reader, Query writer, Install install)
    -> Flow::Status {
  return reader.bind<typename Protocol::View>().visit(
      [&](auto view) {
        return writer.bind<typename Protocol::Access>().visit(
            [&](auto access) {
              const auto& representation = view.get_representation();
              if (!representation.compatible(access.get_representation())) {
                return Flow::Status::Incompatible;
              }

              install(representation, view, access);
              return Flow::Status::Success;
            },
            failure);
      },
      failure);
}
auto Flow::connect(Query reader, Query writer) -> Status {
  Status unavailable = Status::Unsupported;
  auto next = [&](Status result) {
    if (result == Status::Incompatible) {
      unavailable = result;
    }

    return result == Status::Unsupported || result == Status::Incompatible;
  };

  auto result = cooperate<Direct>(
      reader, writer, [&](const Representation& agreed, auto, auto access) {
        representation = &agreed;
        protocol = Protocol::Direct;
        state.direct = access.read_ptr();
      });
  if (!next(result)) {
    return result;
  }

  // Select Shared before acquiring it. A provider failure belongs to that
  // agreement and cannot silently choose a different transport afterward.
  Status acquired = Status::Rejected;
  result = cooperate<Shared>(
      reader, writer, [&](const Representation& agreed, auto, auto access) {
        acquired = access.acquire().visit(
            [&](Ttx::Data::Protocol::Shared::Lifetime& lifetime) {
              representation = &agreed;
              protocol = Protocol::Shared;
              state.shared = lifetime.take_abi();
              return Status::Success;
            },
            [](Ttx::Data::Status status) {
              return static_cast<Status>(status);
            });
      });
  if (result == Status::Success) {
    return acquired;
  }

  if (!next(result)) {
    return result;
  }

  result = cooperate<Block>(
      reader, writer,
      [&](const Representation& agreed, auto view, auto access) {
        representation = &agreed;
        protocol = Protocol::Block;
        state.block = {view.get_abi(), access.get_abi()};
      });
  if (!next(result)) {
    return result;
  }

  result = cooperate<Fragment>(
      reader, writer, [&](const Representation& agreed, auto, auto access) {
        representation = &agreed;
        protocol = Protocol::Fragment;
        state.fragment = access.get_abi();
      });
  return result == Status::Unsupported ? unavailable : result;
}

auto Flow::close() -> void {
  ttx_shared_lifetime lifetime = {};
  if (protocol == Protocol::Shared) {
    lifetime = state.shared;
  }

  protocol = Protocol::None;
  representation = nullptr;
  // Unpublish before entering the owner's release hook. That hook can then
  // establish another agreement without this close overwriting it on return.
  ttx_shared_release(&lifetime);
}

auto ttx_flow_connect(
    ttx_flow* flow,
    ttx_semantic_query reader,
    ttx_semantic_query writer) -> ttx_flow_status {
  return static_cast<ttx_flow_status>(
      reinterpret_cast<Flow*>(flow)->connect(Query(reader), Query(writer)));
}
