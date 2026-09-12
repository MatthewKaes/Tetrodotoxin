# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

# Keep the measurement wrappers in this executable. This package boundary also
# keeps their sources and headers out of the shared validation globs.
package(
    default_testonly = True,
    default_visibility = ["//visibility:private"],
)

# These checks deliberately link Data without Semantic. The transport itself
# must remain usable before a UUID agreement or semantic mapping exists.
cc_binary(
    name = "data_tests",
    srcs = [
        "compiled.cpp",
        "navigation.cpp",
        "preparation.hpp",
        "representation.cpp",
        "schema.cpp",
        "transport.cpp",
        "//validation:unit_test.cpp",
        "//validation:unit_tests/process/child.cpp",
    ],
    deps = [
        "//ttx:data",
        "//validation:test_headers",
        "//validation:process_fixture",
    ],
)

cc_library(
    name = "provider_contract",
    hdrs = [
        "heterogeneous_provider.h",
        "provider.h",
        "provider_representation.h",
    ],
    deps = [
        "//ttx:data",
        "//ttx:semantic",
    ],
)
cc_binary(
    name = "libflow_provider.so",
    srcs = ["provider.c"],
    linkshared = True,
    copts = ["-std=c17", "-Werror", "-Wall", "-Wextra", "-pedantic-errors"],
    deps = [":provider_contract"],
)
cc_binary(
    name = "flow_tests",
    srcs = [
        "block.cpp",
        "bootstrap.cpp",
        "compiled_flow.cpp",
        "copy.cpp",
        "fixtures.cpp",
        "fixtures.hpp",
        "fragment.cpp",
        "measurement.cpp",
        "measurement.hpp",
        "module.cpp",
        "named.cpp",
        "negotiation.cpp",
        "primitives.cpp",
        "preparation.hpp",
        "shared.cpp",
        "swizzle.cpp",
        "//validation:unit_test.cpp",
        "//validation:unit_tests/process/child.cpp",
    ],
    data = [":libflow_provider.so", ":libnamed_provider.so"],
    linkopts = [
        "-ldl",
        "-Wl,--wrap=memmove",
        "-Wl,--wrap=__asan_memmove",
        "-Wl,--wrap=malloc",
        "-Wl,--wrap=calloc",
        "-Wl,--wrap=realloc",
        "-Wl,--wrap=aligned_alloc",
        "-Wl,--wrap=posix_memalign",
    ],
    deps = [
        ":provider_contract",
        "//ttx:concept",
        "//ttx:data",
        "//ttx:semantic",
        "//validation:process_fixture",
        "//validation:test_headers",
    ],
)

cc_binary(
    name = "libnamed_provider.so",
    srcs = ["named_provider.c"],
    linkshared = True,
    copts = ["-std=c17", "-Werror", "-Wall", "-Wextra", "-pedantic-errors"],
    deps = [":provider_contract"],
)
