// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/environment/workspace.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "perimortem/memory/dynamic/record.hpp"
#include "perimortem/memory/managed/bytes.hpp"

#include "perimortem/system/path.hpp"

#include "tetrodotoxin/language/parser/comment.hpp"
#include "tetrodotoxin/language/parser/dialect.hpp"
#include "tetrodotoxin/language/parser/import.hpp"
#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "tetrodotoxin/package/content.hpp"
#include "tetrodotoxin/package/dialect.hpp"
#include "tetrodotoxin/package/resource.hpp"
#include "tetrodotoxin/package/storage.hpp"
#include "tetrodotoxin/source/declaration.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/lexical/cursor.hpp"
#include "ttx/lexical/tokenizer.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::System;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin;

static auto append_storage_failure(
    auto& report,
    const Package::Storage::Failure& failure,
    View::Bytes subject) -> void {
  report << subject;
  failure.get_path().visit(
      [&]() { report << " has an empty or invalid confined path."_view; },
      [&](const Path& path) {
        report << " `"_view << path.get_view() << "` "_view;
        switch (failure.get_error()) {
        case Package::Storage::Failure::Error::InvalidRoute:
          report << "is not a confined logical child."_view;
          break;
        case Package::Storage::Failure::Error::Unreadable:
          report << "could not be read from the opened Package root."_view;
          break;
        default:
          report << "could not be acquired."_view;
          break;
        }
      });
}

Environment::Workspace::Workspace(
    Toolchain& selected_toolchain,
    Option<Dynamic::Record<Package::Snapshots>> selected_snapshots)
    : toolchain(selected_toolchain),
      snapshots(selected_snapshots),
      arena(),
      retained_sources(),
      package_members(arena),
      retained_monographs(),
      packages(arena),
      pending_package_imports() {}

Environment::Workspace::~Workspace() = default;

// Workspace owns acquisition for its retained graphs, so this input pass
// records source imports before interpretation. The bootstrap Toolchain path
// instead leaves the body and its commands to the selected Dialect.
static auto interpret_source(
    Environment::Toolchain& toolchain,
    Cursor& cursor,
    Abstract& context) -> Option<Language::Monograph&> {
  // The common envelope is consumed before protocol dispatch so every Dialect
  // receives the same source documentation and Anchor contract. Concrete
  // grammar begins only after that shared ownership boundary.
  Token source_opening = cursor.current();
  if (!cursor.get_code().is_comment()) {
    cursor.require(
        Code::Type::Comment,
        "Source is missing required documentation comment. Provide at least an "
        "explicit empty comment."_view);
    return {};
  }

  const Documentation& documentation = Language::Parser::Comment::parse(cursor);
  if (documentation.is_empty()) {
    cursor.create_token_error(
        source_opening,
        "Source is missing required documentation comment. Raw comments do "
        "not become Documentation."_view);
    return {};
  }
  Token dialect_declaration = cursor.current();
  View::Bytes dialect_name = Language::Parser::Dialect::parse(cursor);
  if (dialect_name.is_empty()) {
    return {};
  }

  Anchor source_anchor = Anchor::create(
      dialect_declaration, Span(source_opening, cursor.peek(-1)));
  Option<Language::Dialect&> dialect = toolchain.find(dialect_name);
  if (!dialect) {
    // Dispatch is exact installed name routing. Listing the same live instances
    // in the diagnostic avoids a second registry or an implied fallback rule.
    auto report = cursor.create_report(Span(dialect_declaration));
    auto& hint = report.get_hint();

    report << "Unknown dialect "_view << dialect_name
           << " can't be used to interpret this source."_view;
    hint << "Installed dialects: "_view;
    const auto installed = toolchain.get_dialects();
    if (installed.is_empty()) {
      hint << "<None>"_view;
    } else {
      for (Count i = 0; i < installed.get_size(); i++) {
        if (i != 0) {
          hint << ", "_view;
        }
        hint << installed.get_data()[i].get().get_name();
      }
    }
    hint << "."_view;
    return {};
  }

  Managed::Vector<Language::Import::Description> imports(cursor.get_arena());
  while (Language::Parser::Import::is_next(cursor)) {
    const Documentation& import_documentation =
        Language::Parser::Comment::parse(cursor);
    auto import = Language::Parser::Import::parse(cursor, import_documentation);
    if (import) {
      imports.insert(*import);
    } else {
      cursor.recover_to_statement();
    }
  }

  auto interpretation =
      dialect->interpret(cursor, documentation, source_anchor, context);
  BAIL_IF(!interpretation);
  for (const Language::Import::Description& import : imports.get_view()) {
    if (!interpretation->retain_import(import, cursor.get_associations())) {
      cursor.create_expression_error(
          import.get_declaration_anchor(),
          "Source repeats one local Import Type name."_view,
          "Give each imported source or Package one distinct local name."_view);
    }
  }
  return *interpretation;
}

auto Environment::Workspace::interpret_source(
    Errors& errors,
    View::Bytes semantic_name,
    View::Bytes diagnostic_path,
    View::Bytes contents) -> Option<Language::Monograph&> {
  // Source graph objects borrow bytes and Tokens from this candidate Arena.
  // Keeping the lexical and semantic work under one handle lets any rejection
  // release the whole unfinished graph together.
  Dynamic::Record<Allocator::Arena> transaction;
  View::Bytes retained_contents = transaction->proxy(contents);
  View::Bytes retained_path = transaction->proxy(diagnostic_path);
  Tokenizer& tokenizer = transaction->construct<Tokenizer>(
      *transaction, retained_contents, retained_path);
  Associations& associations =
      transaction->construct<Associations>(*transaction);
  Cursor& cursor =
      transaction->construct<Cursor>(tokenizer, errors, associations);

  if (retained_monographs.contains(semantic_name)) {
    cursor.create_error(
        "This semantic source name is already published in the Workspace."_view,
        semantic_name);
    return {};
  }

  Count source_error_count = errors.get_size();
  auto interpretation = ::interpret_source(toolchain, cursor, *this);
  if (!interpretation) {
    if (errors.get_size() == source_error_count) {
      cursor.create_error(
          "Source interpretation failed without a more specific diagnostic."_view);
    }
    return {};
  }
  Language::Monograph& monograph = *interpretation;
  Bool completed = errors.get_size() == source_error_count;

  if (monograph.is<Package::Language::Monograph>()) {
    // A Package root needs its confined path because Workspace must walk and
    // complete every external Type edge before publishing the graph.
    if (completed) {
      cursor.create_error(
          "A Package root must be completed through Workspace Package "
          "import."_view);
    }
    return {};
  }

  // The Monograph proves that the Dialect established a durable source owner.
  // Retaining its transaction here preserves Tokens, Associations, and every
  // partial identity while later barriers decide product eligibility.
  Count retained_index = retained_sources.get_size();
  View::Bytes retained_name = arena.proxy(semantic_name);
  retained_sources.insert({
    .package_root = {},
    .name = retained_name,
    .logical_route = retained_path,
    .diagnostic_path = retained_path,
    .source_text = retained_contents,
    .transaction = transaction,
    .monograph = monograph,
    .tokens = tokenizer.get_tokens(),
    .associations = associations,
    .completed = False,
  });

  // Linking can still enrich a retained graph after interpretation reports an
  // incomplete source form. The earlier report keeps publication closed while
  // editor queries gain any Types and declaration edges that did settle.
  source_error_count = errors.get_size();
  Bool composed = monograph.compose(cursor);
  Bool linked = composed && monograph.link(cursor);
  if (!linked && completed && errors.get_size() == source_error_count) {
    cursor.create_error(
        "Source linking failed without a more specific diagnostic."_view);
  }
  completed &= linked;

  if (completed) {
    source_error_count = errors.get_size();
    if (!monograph.finalize(cursor)) {
      if (errors.get_size() == source_error_count) {
        cursor.create_error(
            "Source finalization failed without a more specific diagnostic."_view);
      }
      completed = False;
    }
  }

  retained_monographs.insert(
      retained_name, Ttx::Concept::Reference<Language::Monograph>(monograph));
  retained_sources[retained_index].completed = completed;
  return completed ? Option<Language::Monograph&>(monograph)
                   : Option<Language::Monograph&>();
}

auto Environment::Workspace::import_package(
    Errors& errors,
    View::Bytes package_root,
    View::Bytes root_semantic_name,
    View::Bytes root_logical_route) -> Option<Language::Monograph&> {
  // Reading the Package root creates the first authored Cursor. Failures before
  // that point belong to Package acquisition, while later failures can use the
  // exact source text and Anchor from that Cursor.
  Allocator::Arena acquisition;

  if (!snapshots) {
    snapshots = Dynamic::Record<Package::Snapshots>();
  }

  auto storage = Package::Storage::open(acquisition, package_root, *snapshots);
  if (!storage) {
    Diagnostics::Log::Message<768> message(Diagnostics::Log::Level::Error);
    message
        << "Package import could not open its confined filesystem root `"_view
        << package_root << "`."_view;
    return {};
  }

  Option<Package::Content&> manifest =
      storage->read(root_logical_route)
          .visit(
              [](Package::Content& content) {
                return Option<Package::Content&>(content);
              },
              [&](const Package::Storage::Failure& failure) {
                Diagnostics::Log::Message<768> message(
                    Diagnostics::Log::Level::Error);
                append_storage_failure(
                    message, failure, "Package root source"_view);
                return Option<Package::Content&>();
              });
  if (!manifest) {
    return {};
  }

  // The Package root begins the candidate graph. Its bytes, Tokens, Cursor, and
  // Package Monograph share one Arena, so a rejected import releases every
  // borrowed view together.
  Dynamic::Record<Allocator::Arena> root_transaction;
  View::Bytes root_contents = root_transaction->proxy(manifest->get_contents());
  View::Bytes root_path =
      root_transaction->proxy(manifest->get_diagnostic_path());
  Tokenizer& root_tokenizer = root_transaction->construct<Tokenizer>(
      *root_transaction, root_contents, root_path);
  Associations& root_associations =
      root_transaction->construct<Associations>(*root_transaction);
  Cursor& root_cursor = root_transaction->construct<Cursor>(
      root_tokenizer, errors, root_associations, root_logical_route);
  if (retained_monographs.contains(root_semantic_name)) {
    root_cursor.create_error(
        "This Package semantic name is already published in the Workspace."_view,
        root_semantic_name);
    return {};
  }

  // The Package root contributes only its Library export surface and common
  // Import Types. Workspace discovers the complete source graph by walking
  // those external Type edges.
  Count root_error_count = errors.get_size();
  auto root_interpretation = ::interpret_source(toolchain, root_cursor, *this);
  if (!root_interpretation) {
    if (errors.get_size() == root_error_count) {
      root_cursor.create_error(
          "Package root interpretation failed without a more specific "
          "diagnostic."_view);
    }
    return {};
  }

  auto selected_root =
      root_interpretation->select<Package::Language::Monograph>();
  if (!selected_root) {
    root_cursor.create_error(
        "The root source of a Package import must use the installed Package "
        "Dialect."_view);
    return {};
  }
  Package::Language::Monograph& root = *selected_root;
  View::Bytes root_package_identity = root.get_name();
  Version root_package_version = root.get_version();
  for (Count i = 0; i < packages.get_size(); i++) {
    const ImportedPackage& imported = packages[i];
    if (imported.identity != root_package_identity) {
      continue;
    }

    root_cursor.create_error(
        imported.version == root_package_version
            ? "This exact Package identity and version is already imported "
              "into the Workspace."_view
            : "This Package identity is already imported with a different "
              "version."_view,
        root_package_identity);
    return {};
  }

  Dynamic::Vector<Dynamic::Record<Allocator::Arena>> candidate_transactions;
  Managed::Vector<Language::Monograph*> candidates(acquisition);
  Managed::Vector<Cursor*> cursors(acquisition);
  Managed::Vector<View::Bytes> diagnostic_paths(acquisition);
  Managed::Vector<View::Bytes> logical_routes(acquisition);
  Managed::Vector<View::Bytes> source_names(acquisition);
  Managed::Vector<Bool> parse_validity(acquisition);
  candidate_transactions.insert(root_transaction);
  candidates.insert(&root);
  cursors.insert(&root_cursor);
  diagnostic_paths.insert(root_path);
  logical_routes.insert(root_logical_route);
  source_names.insert(root_semantic_name);
  parse_validity.insert(errors.get_size() == root_error_count);

  Bool retained = False;
  auto retain_candidates = [&](Bool completed) {
    if (retained) {
      return;
    }

    View::Bytes retained_package_root = arena.proxy(package_root);
    for (Count index = 0; index < candidate_transactions.get_size(); index++) {
      retained_sources.insert({
        .package_root = retained_package_root,
        .name = source_names[index],
        .logical_route = logical_routes[index],
        .diagnostic_path = diagnostic_paths[index],
        .source_text = cursors[index]->get_source_text(),
        .transaction = candidate_transactions[index],
        .monograph = *candidates[index],
        .tokens = cursors[index]->get_tokens(),
        .associations = cursors[index]->get_associations(),
        .completed = completed,
      });
      if (index != 0) {
        package_members.insert({
          .package = &root,
          .name = arena.proxy(source_names[index]),
          .logical_route = arena.proxy(logical_routes[index]),
          .monograph = candidates[index],
        });
      }
    }
    retained_monographs.insert(
        arena.proxy(root_semantic_name),
        Ttx::Concept::Reference<Language::Monograph>(root));
    retained = True;
  };

  // Package resources borrow this import's confined Storage while sources are
  // parsed. Sealing it before linking leaves later semantic stages with only
  // the resources the Package already selected.
  if (!root.get_resources().connect(*storage)) {
    root_cursor.create_error(
        "The Package resource table rejected its one import storage."_view);
    retain_candidates(False);
    return {};
  }

  // Common Import Types own the new source graph. Each local locator is
  // resolved relative to the importing source, while package imports terminate
  // at one already restored exact Package product. Each Import retains that
  // acquisition while its ordinary Type expression selects the visible target.
  Bool parsed = parse_validity[0];
  for (Count candidate_index = 0; candidate_index < candidates.get_size();
       candidate_index++) {
    Language::Monograph& importer = *candidates[candidate_index];
    for (const Reference<Language::Import>& retained_type :
         importer.get_imports()) {
      Language::Import& import = retained_type.get();
      if (import.get_acquired()) {
        continue;
      }

      if (import.get_kind() == Language::Import::Kind::Package) {
        Option<const ImportedPackage&> selected;
        for (Count package_index = 0; package_index < packages.get_size();
             package_index++) {
          const ImportedPackage& candidate = packages[package_index];
          if (candidate.identity == import.get_locator() &&
              candidate.version == import.get_version()) {
            selected = candidate;
            break;
          }
        }
        auto target =
            selected
                ? selected->monograph->get_root().select<Ttx::Model::Type>()
                : Option<const Ttx::Model::Type&>();
        if (!target || !import.acquire(*target)) {
          if (!selected && !pending_package_imports.get_view().contains(
                               [&](const Reference<Language::Import>& pending) {
                                 return pending.get().get_locator() ==
                                            import.get_locator() &&
                                        pending.get().get_version() ==
                                            import.get_version();
                               })) {
            pending_package_imports.emplace(import);
          }
          cursors[candidate_index]->create_expression_error(
              import.get_declaration_anchor(),
              "Package Import did not select one restored exact Package."_view,
              import.get_locator());
          parse_validity[candidate_index] = False;
          parsed = False;
        }
        continue;
      }

      Path route(logical_routes[candidate_index], import.get_locator());
      View::Bytes normalized_route = route.get_view();
      if (normalized_route.is_empty() || route.is_rooted()) {
        cursors[candidate_index]->create_expression_error(
            import.get_declaration_anchor(),
            "Source Import did not resolve to one confined relative path."_view,
            import.get_locator());
        parse_validity[candidate_index] = False;
        parsed = False;
        continue;
      }

      Option<Count> existing_index;
      for (Count index = 0; index < logical_routes.get_size(); index++) {
        if (logical_routes[index] == normalized_route) {
          existing_index = index;
          break;
        }
      }

      if (!existing_index) {
        Option<Package::Content&> content =
            storage->read(normalized_route)
                .visit(
                    [](Package::Content& acquired) {
                      return Option<Package::Content&>(acquired);
                    },
                    [&](const Package::Storage::Failure& failure) {
                      auto report = cursors[candidate_index]->create_report(
                          import.get_declaration_anchor());
                      append_storage_failure(
                          report, failure, "Source Import"_view);
                      return Option<Package::Content&>();
                    });
        if (!content) {
          parse_validity[candidate_index] = False;
          parsed = False;
          continue;
        }

        Dynamic::Record<Allocator::Arena> source_transaction;
        View::Bytes source_contents =
            source_transaction->proxy(content->get_contents());
        View::Bytes source_path =
            source_transaction->proxy(content->get_diagnostic_path());
        View::Bytes retained_route =
            source_transaction->proxy(normalized_route);
        Tokenizer& tokenizer = source_transaction->construct<Tokenizer>(
            *source_transaction, source_contents, source_path);
        Associations& associations =
            source_transaction->construct<Associations>(*source_transaction);
        Cursor& cursor = source_transaction->construct<Cursor>(
            tokenizer, errors, associations, retained_route);
        Count source_error_count = errors.get_size();
        auto interpretation = ::interpret_source(toolchain, cursor, root);
        if (!interpretation) {
          if (errors.get_size() == source_error_count) {
            cursor.create_error(
                "Imported source interpretation failed without a more specific diagnostic."_view);
          }
          parse_validity[candidate_index] = False;
          parsed = False;
          continue;
        }
        if (interpretation->is<Package::Language::Monograph>()) {
          cursor.create_error(
              "A local source Import cannot begin another Package product."_view,
              "Use package(.name = ..., .version = ...) at that boundary."_view);
          parse_validity[candidate_index] = False;
          parsed = False;
          continue;
        }

        candidate_transactions.insert(source_transaction);
        candidates.insert(&*interpretation);
        cursors.insert(&cursor);
        diagnostic_paths.insert(source_path);
        logical_routes.insert(retained_route);
        Managed::Bytes semantic_route(*source_transaction);
        if (candidate_index != 0) {
          semantic_route.concat(source_names[candidate_index]);
          semantic_route.concat("::"_view);
        }
        semantic_route.concat(import.get_name());
        source_names.insert(semantic_route.get_view());
        parse_validity.insert(errors.get_size() == source_error_count);
        existing_index = candidates.get_size() - 1;
      }

      auto target =
          candidates[*existing_index]->get_root().select<Ttx::Model::Type>();
      if (!target || !import.acquire(*target)) {
        cursors[candidate_index]->create_expression_error(
            import.get_declaration_anchor(),
            "Source Import Type could not acquire its semantic root."_view,
            import.get_locator());
        parse_validity[candidate_index] = False;
        parsed = False;
      }
    }
  }

  // Source parsing is where Package Storage becomes authored language facts.
  // Linking receives the sealed context after that conversion, when the set of
  // semantic candidates is already fixed.
  root.get_resources().seal();

  // External source Types determine completion order. Parsing established every
  // identity already. This dependency order lets each imported
  // source settle its generated and authored Types before an importer validates
  // those layouts. Package imports terminate at graphs restored earlier.
  Managed::Vector<U8> source_states(acquisition);
  Managed::Vector<Count> source_order(acquisition);
  for (Count index = 0; index < candidates.get_size(); index++) {
    source_states.insert(0);
  }
  auto order_source = [&](auto& self, Count index) -> Bool {
    if (source_states[index] == 2) {
      return True;
    }
    if (source_states[index] == 1) {
      cursors[index]->create_error(
          "Source Import graph contains a cycle."_view,
          "Break the cycle or place the shared Types in a third source."_view);
      return False;
    }
    source_states[index] = 1;
    for (const Reference<Language::Import>& retained_type :
         candidates[index]->get_imports()) {
      Language::Import& import = retained_type.get();
      if (import.get_kind() != Language::Import::Kind::Source) {
        continue;
      }
      auto acquired = import.get_acquired();
      Option<Count> target_index;
      for (Count candidate_index = 0; candidate_index < candidates.get_size();
           candidate_index++) {
        if (acquired &&
            &candidates[candidate_index]->get_root() == &*acquired) {
          target_index = candidate_index;
          break;
        }
      }
      if (!target_index || !self(self, *target_index)) {
        return False;
      }
    }
    source_states[index] = 2;
    source_order.insert(index);
    return True;
  };
  for (Count index = 0; index < candidates.get_size(); index++) {
    if (!order_source(order_source, index)) {
      parsed = False;
    }
  }

  // Composition follows installed Dialect order after every member identity is
  // present. Dependency Dialects can publish inherited declarations before an
  // unrelated Library consumer links, so Package source order never chooses
  // the available semantic surface.
  Bool composed = parsed;
  for (const Reference<Language::Dialect>& dialect : toolchain.get_dialects()) {
    for (Count i = 0; i < candidates.get_size(); i++) {
      if (&candidates[i]->get_language() != &dialect.get()) {
        continue;
      }

      Count source_error_count = errors.get_size();
      Bool candidate_composed = candidates[i]->compose(*cursors[i]);
      if (!candidate_composed) {
        if (parse_validity[i] && errors.get_size() == source_error_count) {
          cursors[i]->create_error(
              "Package source composition failed without a more specific "
              "diagnostic."_view);
        }
        composed = False;
      }
    }
  }

  // Ordinary linking still visits every retained candidate so tooling keeps
  // the strongest graph it can observe. Publication remains closed when any
  // earlier composition or parse step failed.
  Bool linked = composed;
  for (Count ordered = 0; ordered < source_order.get_size(); ordered++) {
    Count i = source_order[ordered];
    Count source_error_count = errors.get_size();
    Bool imports_resolved = True;
    for (const Reference<Language::Import>& retained_type :
         candidates[i]->get_imports()) {
      imports_resolved &= retained_type.get().validate(*cursors[i]);
    }
    Bool candidate_linked =
        imports_resolved && candidates[i]->link(*cursors[i]);
    if (!candidate_linked) {
      if (parse_validity[i] && errors.get_size() == source_error_count) {
        cursors[i]->create_error(
            "Package source linking failed without a more specific "
            "diagnostic."_view);
      }
      linked = False;
    }
  }

  // Finalization can consume linked declarations from any member. Waiting for
  // the whole graph to link gives each candidate the same completed context.
  Bool finalized = linked;
  if (linked) {
    for (Count ordered = 0; ordered < source_order.get_size(); ordered++) {
      Count i = source_order[ordered];
      Count source_error_count = errors.get_size();
      if (!candidates[i]->finalize(*cursors[i])) {
        if (errors.get_size() == source_error_count) {
          cursors[i]->create_error(
              "Package source finalization failed without a more specific "
              "diagnostic."_view);
        }
        finalized = False;
      }
    }
  }

  retain_candidates(finalized);
  if (!finalized) {
    return {};
  }

  View::Bytes retained_identity = arena.proxy(root_package_identity);
  packages.insert({
    .identity = retained_identity,
    .version = root_package_version,
    .monograph = &root,
  });
  return root;
}

auto Environment::Workspace::restore_package(
    const Package::Archive::Archive& archive,
    View::Bytes root_semantic_name) -> Option<Language::Monograph&> {
  if (root_semantic_name.is_empty() ||
      retained_monographs.contains(root_semantic_name)) {
    Diagnostics::Log::error(
        "Package restoration requires one unpublished semantic name."_view);
    return {};
  }

  for (Count index = 0; index < packages.get_size(); index++) {
    const ImportedPackage& imported = packages[index];
    if (imported.identity == archive.get_identity()) {
      Diagnostics::Log::error(
          "Package restoration cannot publish one identity twice."_view);
      return {};
    }
  }

  Dynamic::Record<Allocator::Arena> root_transaction;
  auto package_dialect = toolchain.find("Package"_view);
  if (!package_dialect || !package_dialect->is<Package::Dialect>()) {
    Diagnostics::Log::error(
        "Package restoration requires the installed Package Dialect."_view);
    return {};
  }
  Managed::Vector<Reference<Package::Resource>> resources(*root_transaction);
  for (const Package::Archive::Resource& archived : archive.get_resources()) {
    resources.insert(
        Package::Resource::create(
            *root_transaction, archived.get_route(), archived.get_value()));
  }
  Package::Language::Monograph& root =
      Package::Language::Monograph::create_synthetic(
          *root_transaction, *package_dialect, archive.get_identity(),
          archive.get_version(), *this,
          static_cast<Package::Dialect&>(*package_dialect).get_library(),
          resources.get_view());

  Dynamic::Vector<Dynamic::Record<Allocator::Arena>> candidates;
  Managed::Vector<Language::Monograph*> monographs(*root_transaction);
  Managed::Vector<View::Bytes> restored_member_names(*root_transaction);
  Managed::Map<View::Bytes, Language::Monograph&> restored_members(
      *root_transaction);
  candidates.insert(root_transaction);
  monographs.insert(&root);
  for (const Package::Archive::Member& member : archive.get_members()) {
    auto dialect = toolchain.find(member.get_dialect_name());
    if (!dialect) {
      Diagnostics::Log::error(
          "Package restoration requires every member Dialect installed."_view);
      return {};
    }

    if (member.get_semantic_name() == "PackageSurface"_view) {
      Diagnostics::Log::error(
          "Package Library projection decoding is not implemented."_view);
      return {};
    }

    Dynamic::Record<Allocator::Arena> transaction;
    auto decoded = dialect->decode(*transaction, member.get_payload(), root);
    // This legacy completion path uses native Monograph operations. A decoder
    // may return a different simulacrum, so check that capability before use.
    auto restored = decoded ? decoded->select<Language::Monograph>()
                            : Option<Language::Monograph&>();
    View::Bytes member_name =
        root_transaction->proxy(member.get_semantic_name());
    if (!restored || restored->is<Package::Language::Monograph>()) {
      Diagnostics::Log::Message<256> message(
          Diagnostics::Log::Level::Error, Diagnostics::Source());
      message << "Package restoration rejected member `"_view << member_name
              << "` for the "_view << member.get_dialect_name()
              << " Dialect."_view;
      return {};
    }

    candidates.insert(transaction);
    monographs.insert(&*restored);
    restored_members.launder(member_name, *restored);
    restored_member_names.insert(member_name);
  }

  for (const Package::Archive::GraphImport& archived : archive.get_imports()) {
    Language::Monograph* importer = nullptr;
    if (archived.get_importer() == "PackageSurface"_view) {
      importer = &root;
    } else {
      auto selected = restored_members.find(archived.get_importer());
      if (selected) {
        importer = &selected->value;
      }
    }
    if (!importer) {
      Diagnostics::Log::error(
          "Package restoration could not select one Import owner."_view);
      return {};
    }

    Language::Import::Description description(
        root_transaction->proxy(archived.get_local_name()),
        Documentation::get_empty(), archived.get_visibility(),
        archived.get_kind(), root_transaction->proxy(archived.get_target()),
        archived.get_version(), root_transaction->proxy(archived.get_route()),
        Anchor::create(Span()), Anchor::create(Span()), Anchor::create(Span()));
    if (!importer->retain_import(description)) {
      Diagnostics::Log::error(
          "Package restoration could not retain one Import Type."_view);
      return {};
    }
    Language::Import& import =
        importer->get_imports()
            .get_data()[importer->get_imports().get_size() - 1]
            .get();

    const Ttx::Model::Type* target = nullptr;
    if (archived.get_kind() == Language::Import::Kind::Source) {
      auto selected = restored_members.find(archived.get_target());
      if (selected) {
        auto root_type = selected->value.get_root().select<Ttx::Model::Type>();
        if (root_type) {
          target = &*root_type;
        }
      }
    } else {
      for (const ImportedPackage& candidate : packages.get_view()) {
        if (candidate.identity == archived.get_target() &&
            candidate.version == archived.get_version()) {
          auto root_type =
              candidate.monograph->get_root().select<Ttx::Model::Type>();
          if (root_type) {
            target = &*root_type;
          }
          break;
        }
      }
    }
    if (!target || !import.acquire(*target)) {
      Diagnostics::Log::error(
          "Package restoration could not acquire one external Type."_view);
      return {};
    }
  }

  Managed::Vector<U8> restored_states(*root_transaction);
  Managed::Vector<Count> restored_order(*root_transaction);
  for (Count index = 0; index < monographs.get_size(); index++) {
    restored_states.insert(0);
  }
  auto order_restored = [&](auto& self, Count index) -> Bool {
    if (restored_states[index] == 2) {
      return True;
    }
    BAIL_IF(restored_states[index] == 1);
    restored_states[index] = 1;
    for (const Reference<Language::Import>& retained_type :
         monographs[index]->get_imports()) {
      Language::Import& import = retained_type.get();
      if (import.get_kind() != Language::Import::Kind::Source) {
        continue;
      }
      auto acquired = import.get_acquired();
      Option<Count> target_index;
      for (Count candidate = 0; candidate < monographs.get_size();
           candidate++) {
        if (acquired && &monographs[candidate]->get_root() == &*acquired) {
          target_index = candidate;
          break;
        }
      }
      BAIL_IF(!target_index || !self(self, *target_index));
    }
    restored_states[index] = 2;
    restored_order.insert(index);
    return True;
  };
  for (Count index = 0; index < monographs.get_size(); index++) {
    if (!order_restored(order_restored, index)) {
      Diagnostics::Log::error(
          "Package restoration rejected a cyclic Source Import graph."_view);
      return {};
    }
  }

  for (const Reference<Language::Dialect>& dialect : toolchain.get_dialects()) {
    for (Count index = 0; index < monographs.get_size(); index++) {
      if (&monographs[index]->get_language() != &dialect.get()) {
        continue;
      }
      if (!monographs[index]->compose_restored()) {
        Diagnostics::Log::Message<256> message(
            Diagnostics::Log::Level::Error, Diagnostics::Source());
        message << "Package restoration failed while composing `"_view
                << monographs[index]->get_name() << "` member `"_view
                << (index == 0 ? "PackageSurface"_view
                               : restored_member_names[index - 1])
                << "`."_view;
        return {};
      }
    }
  }

  for (Count ordered = 0; ordered < restored_order.get_size(); ordered++) {
    Count index = restored_order[ordered];
    for (const Reference<Language::Import>& retained_type :
         monographs[index]->get_imports()) {
      Language::Import& import = retained_type.get();
      if (!import.validate_restored()) {
        Diagnostics::Log::Message<256> message(
            Diagnostics::Log::Level::Error, Diagnostics::Source());
        message << "Package restoration could not resolve external Type `"_view
                << import.get_name() << "` from `"_view << import.get_locator()
                << "`"_view;
        if (!import.get_route().is_empty()) {
          message << "::"_view << import.get_route();
        }
        message << "."_view;
        return {};
      }
    }

    if (!monographs[index]->link_restored()) {
      Diagnostics::Log::error(
          "Package restoration failed while linking member graphs."_view);
      return {};
    }
  }
  for (Count ordered = 0; ordered < restored_order.get_size(); ordered++) {
    Count index = restored_order[ordered];
    if (!monographs[index]->finalize_restored()) {
      Diagnostics::Log::error(
          "Package restoration failed while finalizing member graphs."_view);
      return {};
    }
  }

  for (const Dynamic::Record<Allocator::Arena>& candidate :
       candidates.get_view()) {
    restored_transactions.insert(candidate);
  }
  for (Count index = 0; index < restored_member_names.get_size(); index++) {
    View::Bytes name = arena.proxy(restored_member_names[index]);
    package_members.insert({
      .package = &root,
      .name = name,
      .logical_route = name,
      .monograph = monographs[index + 1],
    });
  }
  View::Bytes retained_identity = arena.proxy(archive.get_identity());
  packages.insert({
    .identity = retained_identity,
    .version = archive.get_version(),
    .monograph = &root,
  });
  View::Bytes retained_name = arena.proxy(root_semantic_name);
  retained_monographs.insert(
      retained_name, Ttx::Concept::Reference<Language::Monograph>(root));
  return root;
}

auto Environment::Workspace::get_associations(View::Bytes diagnostic_path) const
    -> Option<const Associations&> {
  for (Count i = 0; i < retained_sources.get_size(); i++) {
    const RetainedSource& source = retained_sources[i];
    if (source.diagnostic_path == diagnostic_path) {
      return source.associations;
    }
  }

  return {};
}

auto Environment::Workspace::get_associations(
    View::Bytes package_root,
    View::Bytes logical_route) const -> Option<const Associations&> {
  const RetainedSource* source =
      find_retained_source(package_root, logical_route);
  return source ? Option<const Associations&>(source->associations)
                : Option<const Associations&>();
}

auto Environment::Workspace::get_monograph(View::Bytes diagnostic_path) const
    -> Option<const Language::Monograph&> {
  for (const RetainedSource& source : retained_sources.get_view()) {
    if (source.diagnostic_path == diagnostic_path) {
      return source.monograph;
    }
  }
  return {};
}

auto Environment::Workspace::get_monograph(
    View::Bytes package_root,
    View::Bytes logical_route) const -> Option<const Language::Monograph&> {
  const RetainedSource* source =
      find_retained_source(package_root, logical_route);
  return source ? Option<const Language::Monograph&>(source->monograph)
                : Option<const Language::Monograph&>();
}

auto Environment::Workspace::get_completed_monograph(
    View::Bytes diagnostic_path) const -> Option<const Language::Monograph&> {
  for (const RetainedSource& source : retained_sources.get_view()) {
    if (source.diagnostic_path == diagnostic_path && source.completed) {
      return source.monograph;
    }
  }
  return {};
}

auto Environment::Workspace::get_completed_monograph(
    View::Bytes package_root,
    View::Bytes logical_route) const -> Option<const Language::Monograph&> {
  const RetainedSource* source =
      find_retained_source(package_root, logical_route);
  return source && source->completed
             ? Option<const Language::Monograph&>(source->monograph)
             : Option<const Language::Monograph&>();
}

auto Environment::Workspace::get_package_source_count(
    const Package::Language::Monograph& package) const -> Count {
  Count count = 0;
  for (const PackageMember& member : package_members.get_view()) {
    if (member.package == &package) {
      count++;
    }
  }
  return count;
}

auto Environment::Workspace::get_package_source(
    const Package::Language::Monograph& package,
    Count selected_index) const -> Option<PackageSource> {
  Count index = 0;
  for (const PackageMember& member : package_members.get_view()) {
    if (member.package != &package) {
      continue;
    }
    if (index++ == selected_index) {
      return PackageSource(
          member.name, member.logical_route, *member.monograph);
    }
  }
  return {};
}

auto Environment::Workspace::get_tokens(View::Bytes diagnostic_path) const
    -> View::Vector<Token> {
  for (const RetainedSource& source : retained_sources.get_view()) {
    if (source.diagnostic_path == diagnostic_path) {
      return source.tokens;
    }
  }
  return {};
}

auto Environment::Workspace::get_tokens(
    View::Bytes package_root,
    View::Bytes logical_route) const -> View::Vector<Token> {
  const RetainedSource* source =
      find_retained_source(package_root, logical_route);
  return source ? source->tokens : View::Vector<Token>();
}

auto Environment::Workspace::find_retained_source(
    View::Bytes package_root,
    View::Bytes logical_route) const -> const RetainedSource* {
  for (Count index = 0; index < retained_sources.get_size(); index++) {
    const RetainedSource& source = retained_sources[index];
    if (source.package_root == package_root &&
        source.logical_route == logical_route) {
      return &source;
    }
  }

  return nullptr;
}

auto Environment::Workspace::get_associations(
    const Language::Monograph& monograph) const -> Option<const Associations&> {
  for (Count i = 0; i < retained_sources.get_size(); i++) {
    const RetainedSource& source = retained_sources[i];
    if (&source.monograph == &monograph) {
      return source.associations;
    }
  }

  return {};
}

auto Environment::Workspace::find_authored_location(
    const Abstract& semantic) const -> Option<AuthoredLocation> {
  Option<Anchor> declaration;
  semantic.bind<Tetrodotoxin::Source::Declaration>().visit(
      [&](const Tetrodotoxin::Source::Declaration::Handle& source) {
        declaration = source.get_anchor();
      },
      [](Binding::Failure) {});

  if (declaration) {
    Token focus = declaration->get_token();
    Span span = declaration->get_span();
    for (const RetainedSource& source : retained_sources.get_view()) {
      for (const Associations::Entry& entry :
           source.associations.get_entries()) {
        if (&entry.get_semantic() != &semantic) {
          continue;
        }
        Anchor candidate = entry.get_anchor();
        Token candidate_focus = candidate.get_token();
        Span candidate_span = candidate.get_span();
        Bool same_focus =
            bool(focus) == bool(candidate_focus) &&
            (!focus || (focus.get_offset() == candidate_focus.get_offset() &&
                        focus.get_size() == candidate_focus.get_size() &&
                        focus.get_code() == candidate_focus.get_code()));
        Bool same_span =
            bool(span) == bool(candidate_span) &&
            (!span || (span.get_offset() == candidate_span.get_offset() &&
                       span.get_size() == candidate_span.get_size()));
        if ((focus && same_focus) || (!focus && same_span)) {
          return AuthoredLocation(
              source.package_root, source.diagnostic_path, source.source_text,
              candidate);
        }
      }
    }
  }

  for (const RetainedSource& source : retained_sources.get_view()) {
    auto anchor = source.associations.find(semantic);
    if (anchor) {
      return AuthoredLocation(
          source.package_root, source.diagnostic_path, source.source_text,
          *anchor);
    }
    // A source root has no declaration Token of its own. Native compilation
    // still needs its physical input and bytes for diagnostics and debugging.
    if (&source.monograph == &semantic ||
        &source.monograph.get_root() == &semantic) {
      return AuthoredLocation(
          source.package_root, source.diagnostic_path, source.source_text,
          Anchor::create(Span()));
    }
  }

  return {};
}

auto Environment::Workspace::find_acquired_location(
    View::Bytes package_root,
    View::Bytes logical_route,
    Count offset,
    const Abstract& semantic) const -> Option<AuthoredLocation> {
  const RetainedSource* importer =
      find_retained_source(package_root, logical_route);
  BAIL_IF(importer == nullptr);

  Token selected;
  for (const Token& token : importer->tokens) {
    Count start = token.get_offset();
    Count end = start + token.get_size();
    if (offset >= start && offset < end) {
      selected = token;
      break;
    }
  }
  BAIL_IF(!selected);

  Code::Type code = selected.get_code().get_type();
  if (code == Code::Type::Source || code == Code::Type::Package) {
    auto import = semantic.select<Language::Import>();
    BAIL_IF(
        !import ||
        (code == Code::Type::Source &&
         import->get_kind() != Language::Import::Kind::Source) ||
        (code == Code::Type::Package &&
         import->get_kind() != Language::Import::Kind::Package));

    auto target = import->get_acquired();
    BAIL_IF(!target);
    for (const RetainedSource& source : retained_sources.get_view()) {
      if (&source.monograph.get_root() == &*target) {
        return AuthoredLocation(
            source.package_root, source.diagnostic_path, source.source_text,
            Anchor::create(Span()));
      }
    }

    return {};
  }

  BAIL_IF(code != Code::Type::Embedded);
  auto resource = semantic.select<Package::Resource>();
  BAIL_IF(!resource);

  auto package = importer->monograph.select<Package::Language::Monograph>();
  const Package::Language::Monograph* owner = package ? &*package : nullptr;
  if (owner == nullptr) {
    for (const PackageMember& member : package_members.get_view()) {
      if (member.monograph == &importer->monograph) {
        owner = member.package;
        break;
      }
    }
  }
  BAIL_IF(owner == nullptr);

  Bool retained = owner->get_resources().get_values().contains(
      [&](const Reference<Package::Resource>& candidate) {
        return &candidate.get() == &*resource;
      });
  BAIL_IF(!retained);
  return AuthoredLocation(
      importer->package_root, resource->get_route(), {},
      Anchor::create(Span()));
}

auto Environment::Workspace::get_name() const -> View::Bytes {
  return "Workspace"_view;
}

auto Environment::Workspace::get_documentation() const -> const Documentation& {
  return Documentation::get_empty();
}

auto Environment::Workspace::resolve() const -> const Abstract& {
  return *this;
}

auto Environment::Workspace::resolve_concept(View::Bytes route) const
    -> const Abstract& {
  return retained_monographs.visit(
      route,
      [](const Reference<Language::Monograph>& selected) -> const Abstract& {
        return selected.get();
      },
      []() -> const Abstract& { return Unknown::get_unknown(); });
}

auto Environment::Workspace::visit_concepts(
    Ttx::Concept::Abstract::Visitor visitor) const -> void {
  for (Count index = 0; index < retained_monographs.get_size(); index++) {
    const auto* entry = retained_monographs.get_entry(index);
    if (entry != nullptr) {
      visitor(entry->key, entry->value.get());
    }
  }
  for (const PackageMember& member : package_members.get_view()) {
    visitor(member.name, *member.monograph);
  }
}
