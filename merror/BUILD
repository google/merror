# Description:
#   C++ library for handling and propagating errors generically and safely.

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "merror",
    hdrs = ["merror.h"],
    deps = [
        ":macros",
        "//merror/domain:default",
    ],
)

cc_library(
    name = "types",
    srcs = ["types.cc"],
    hdrs = ["types.h"],
    deps = [
    ],
)

cc_test(
    name = "types_test",
    size = "small",
    srcs = ["types_test.cc"],
    deps = [
        ":types",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    deps = [
        ":types",
        "//merror/internal:expand_expr",
        "//merror/internal:preprocessor",
        "//merror/internal:tls_map",
    ],
)

cc_test(
    name = "macros_test",
    size = "small",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        "@googletest//:gtest_main",
    ],
)
