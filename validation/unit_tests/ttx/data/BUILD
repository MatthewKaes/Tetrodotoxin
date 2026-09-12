# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(
    default_testonly = True,
    default_visibility = ["//visibility:private"],
)

# Preparation is shared with Semantic examples, but depends only on Data.
# The test owner retains every compiled descriptor through the operation.
cc_library(
    name = "preparation",
    hdrs = ["form/preparation.hpp"],
    deps = ["//ttx:data", "//validation:test_headers"],
    visibility = ["//validation/unit_tests/ttx/semantic:__pkg__"],
)

# These checks link Data without Semantic. Protocol use must remain possible
# before a UUID agreement or semantic operation exists.
cc_binary(
    name = "data_tests",
    srcs = [
        "form/compiled.cpp",
        "form/encoding.cpp",
        "form/navigation.cpp",
        "form/visitation.c",
        "form/visitation.h",
        "form/representation.cpp",
        "form/schema.cpp",
        "form/storage.cpp",
        "protocol/direct.cpp",
        "protocol/fragment.cpp",
        "//validation:unit_test.cpp",
        "//validation:unit_tests/process/child.cpp",
    ],
    deps = [
        ":preparation",
        "//ttx:data",
        "//validation:test_headers",
        "//validation:process_fixture",
    ],
)
