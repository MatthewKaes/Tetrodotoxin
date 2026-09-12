# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

"""Runtime realization for completed App products."""

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "application",
    srcs = glob(["**/*.cpp"]),
    hdrs = glob(["**/*.hpp"]),
    deps = [
        "//perimortem:abi",
        "//perimortem:core",
        "//perimortem:graphics",
        "//perimortem:memory",
        "//perimortem:platform",
        "//perimortem:system",
        "//perimortem:vulkan",
        "//tetrodotoxin/graphics/runtime:runtime",
    ],
)
