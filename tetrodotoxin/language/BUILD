# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

"""Shared Tetrodotoxin language contracts and TTX parser fragments."""

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "language",
    srcs = glob(["**/*.cpp"]),
    hdrs = glob(["**/*.hpp"]),
    deps = [
        "//tetrodotoxin/source",
        "//perimortem:headless",
        "//ttx:concept",
        "//ttx:lexical",
        "//ttx:model",
        "//ttx:semantic",
    ],
)
