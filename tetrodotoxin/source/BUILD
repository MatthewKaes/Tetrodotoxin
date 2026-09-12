# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "source",
    hdrs = ["declaration.hpp"],
    deps = [
        "//ttx:concept",
        "//ttx:lexical",
        "//ttx:semantic",
    ],
)
