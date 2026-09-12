# # Tetrodotoxin
# Copyright (c) 2023-present Matt Kaes and contributors

"""
Perimortem::Core - Home to all common utilities and classes
"""

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "perimortem",
    includes = ["."],
    deps = [
        ":abi",
        ":compression",
        ":core",
        ":graphics",
        ":math",
        ":memory",
        ":platform",
        ":serialization",
        ":system",
        ":vulkan",
    ],
)

cc_library(
    name = "headless",
    deps = [
        ":abi",
        ":serialization",
    ],
)

cc_library(
    name = "abi",
    srcs = glob(["abi/**/*.cpp"]),
    hdrs = glob(["abi/**/*.hpp"]),
    copts = ["-Wreturn-type-c-linkage"],
    includes = ["."],
    deps = [
        ":core",
        ":graphics",
        ":math",
        ":memory",
        ":system",
    ],
)

cc_library(
    name = "core",
    srcs = glob(["core/**/*.cpp"]),
    hdrs = glob([
        "core/**/*.hpp",
        "core/**/*.h",
    ]),
    includes = ["."],
)

cc_library(
    name = "utility",
    srcs = [],
    hdrs = glob([
        "utility/*.hpp",
    ]),
    includes = ["."],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "math",
    hdrs = glob(["math/**/*.hpp"]),
    includes = ["."],
    deps = [":core"],
)

cc_library(
    name = "memory",
    srcs = glob(
        [
            "memory/**/*.cpp",
        ],
        allow_empty = True,
    ),
    hdrs = glob([
        "memory/**/*.hpp",
    ]),
    includes = ["."],
    deps = [
        ":core",
        ":utility",
    ],
)

cc_library(
    name = "compression",
    srcs = glob([
        "compression/**/*.cpp",
    ]),
    hdrs = glob([
        "compression/**/*.hpp",
    ]),
    includes = ["."],
    deps = [
        ":core",
        ":memory",
    ],
)

cc_library(
    name = "system",
    srcs = glob([
        "system/**/*.cpp",
    ]),
    hdrs = glob([
        "system/**/*.hpp",
        "system/**/*.h",
    ]),
    includes = ["."],
    linkopts = ["-lpthread"],
    deps = [
        ":core",
        ":memory",
        ":utility",
    ],
)

cc_library(
    name = "graphics",
    srcs = glob([
        "graphics/**/*.cpp",
    ]),
    hdrs = glob([
        "graphics/**/*.hpp",
    ]),
    includes = ["."],
    deps = [
        ":compression",
        ":core",
        ":memory",
    ],
)

cc_library(
    name = "vulkan",
    srcs = glob(["vulkan/**/*.cpp"]),
    hdrs = glob(["vulkan/**/*.hpp"]),
    includes = ["."],
    linkopts = ["-lvulkan"],
    deps = [
        ":core",
        ":graphics",
        ":memory",
        ":system",
    ],
)

cc_library(
    name = "platform",
    srcs = glob([
        "platform/**/*.cpp",
    ]),
    hdrs = glob([
        "platform/**/*.hpp",
    ]),
    includes = ["."],
    linkopts = ["-lwayland-client"],
    deps = [
        ":core",
        ":system",
    ],
)

cc_library(
    name = "serialization",
    srcs = [
        "serialization/base64.cpp",
        "serialization/json/blueprint.cpp",
        "serialization/json/node.cpp",
        "serialization/stream/binary.cpp",
        "serialization/stream/textual.cpp",
    ],
    hdrs = glob([
        "serialization/*.hpp",
        "serialization/json/*.hpp",
        "serialization/stream/*.hpp",
    ]),
    includes = ["."],
    deps = [
        ":core",
        ":memory",
    ],
)
