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
    name = "abi_core",
    srcs = glob(["abi/core/*.cpp"]) + ["abi/log.cpp"],
    hdrs = glob(["abi/core/*.hpp"]),
    copts = ["-Wreturn-type-c-linkage"],
    includes = ["."],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "abi_memory",
    hdrs = glob(["abi/memory/**/*.hpp"]),
    includes = ["."],
    deps = [
        ":core",
        ":memory",
    ],
)

cc_library(
    name = "abi_graphics",
    srcs = glob(["abi/graphics/**/*.cpp"]),
    hdrs = [],
    copts = ["-Wreturn-type-c-linkage"],
    includes = ["."],
    deps = [
        ":abi_core",
        ":core",
        ":graphics",
    ],
)

cc_library(
    name = "abi_system",
    srcs = glob(["abi/system/*.cpp"]),
    hdrs = glob(["abi/system/*.hpp"]),
    copts = ["-Wreturn-type-c-linkage"],
    includes = ["."],
    deps = [
        ":abi_core",
        ":abi_memory",
        ":core",
        ":math",
        ":memory",
        ":system",
    ],
)

cc_library(
    name = "abi",
    deps = [
        ":abi_core",
        ":abi_graphics",
        ":abi_memory",
        ":abi_system",
    ],
)

cc_library(
    name = "core",
    srcs = [
        "core/algorithm/search.cpp",
        "core/bibliotheca.cpp",
        "core/diagnostics/log.cpp",
        "core/diagnostics/source.cpp",
        "core/implementation.cpp",
        "core/object.cpp",
        "core/reader/binary.cpp",
        "core/reader/serial.cpp",
        "core/reader/textual.cpp",
        "core/thread/worker.cpp",
        "core/time.cpp",
        "core/writer/binary.cpp",
        "core/writer/serial.cpp",
        "core/writer/textual.cpp",
    ],
    hdrs = glob([
        "core/access/*.hpp",
        "core/static/*.hpp",
        "core/view/*.hpp",
        "core/view/*.h",
        "core/algorithm/*.hpp",
        "core/reader/*.hpp",
        "core/writer/*.hpp",
        "core/diagnostics/*.hpp",
        "core/thread/*.hpp",
        "core/*.hpp",
        "core/*.h",
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
        "system/*.cpp",
    ]),
    hdrs = glob(
        [
            "system/*.hpp",
            "system/*.h",
        ],
        exclude = [
            "system/window.hpp",
        ],
    ),
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
        "system/platform/wayland/*.cpp",
    ]),
    hdrs = ["system/window.hpp"] + glob([
        "system/platform/wayland/*.hpp",
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
