# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

"""
The Library Dialect bundle
"""

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "language",
    srcs = glob([
        "builtin/**/*.cpp",
        "language/**/*.cpp",
    ]),
    hdrs = glob([
        "builtin/**/*.hpp",
        "language/**/*.hpp",
    ]),
    deps = [
        "//perimortem:headless",
        "//tetrodotoxin/language",
        "//tetrodotoxin/source",
        "//ttx:concept",
        "//ttx:lexical",
        "//ttx:model",
        "//ttx:semantic",
    ],
)

cc_library(
    name = "interpreter",
    srcs = glob([
        "interpreter/**/*.cpp",
    ]),
    hdrs = glob([
        "interpreter/**/*.hpp",
    ]),
    deps = [
        ":language",
        "//perimortem:headless",
        "//tetrodotoxin/language",
        "//ttx:concept",
        "//ttx:lexical",
        "//ttx:model",
    ],
)

cc_library(
    name = "simulacra",
    srcs = ["simulacra.cpp"],
    hdrs = ["simulacra.hpp"],
    deps = [
        ":language",
        "//perimortem:headless",
        "//tetrodotoxin/language",
        "//ttx:concept",
        "//ttx:lexical",
        "//ttx:model",
        "//ttx:semantic",
    ],
)

cc_library(
    name = "library",
    srcs = ["dialect.cpp"],
    hdrs = ["dialect.hpp"],
    deps = [
        ":interpreter",
        ":language",
        ":simulacra",
        "//perimortem:headless",
        "//tetrodotoxin/language",
        "//ttx:concept",
        "//ttx:lexical",
        "//ttx:model",
    ],
)
