# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

# Measurement wraps process allocation and memmove. Keeping this package and
# executable separate prevents those hooks from entering general validation.
package(
    default_testonly = True,
    default_visibility = ["//visibility:private"],
)

cc_library(
    name = "provider_contract",
    hdrs = [
        "fixtures/heterogeneous_provider.h",
        "fixtures/provider.h",
        "fixtures/provider_representation.h",
    ],
    deps = ["//ttx:data", "//ttx:semantic"],
)

cc_binary(
    name = "libflow_provider.so",
    srcs = ["fixtures/provider.c"],
    linkshared = True,
    copts = ["-std=c17", "-Werror", "-Wall", "-Wextra", "-pedantic-errors"],
    deps = [":provider_contract"],
)

cc_binary(
    name = "libheterogeneous_provider.so",
    srcs = ["fixtures/heterogeneous_provider.c"],
    linkshared = True,
    copts = ["-std=c17", "-Werror", "-Wall", "-Wextra", "-pedantic-errors"],
    deps = [":provider_contract"],
)

cc_binary(
    name = "flow_tests",
    srcs = [
        "block.cpp",
        "query.cpp",
        "flow.cpp",
        "flows/compiled_copy.cpp",
        "flows/copy.cpp",
        "flows/heterogeneous.cpp",
        "flows/swizzle.cpp",
        "fixtures.cpp",
        "fixtures.hpp",
        "fragment.cpp",
        "measurement.cpp",
        "measurement.hpp",
        "module.cpp",
        "module.hpp",
        "operations/fragment.cpp",
        "shared.cpp",
        "//validation:unit_test.cpp",
        "//validation:unit_tests/process/child.cpp",
    ],
    data = [":libflow_provider.so", ":libheterogeneous_provider.so"],
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
        "//validation/unit_tests/ttx/data:preparation",
        "//ttx:concept",
        "//ttx:data",
        "//ttx:semantic",
        "//validation:process_fixture",
        "//validation:test_headers",
    ],
)
