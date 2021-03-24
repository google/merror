package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "tls_map",
    hdrs = ["tls_map.h"],
    deps = [
    ],
)

cc_test(
    name = "tls_map_test",
    size = "small",
    srcs = ["tls_map_test.cc"],
    deps = [
        ":tls_map",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "expand_expr",
    hdrs = ["expand_expr.h"],
    deps = [
        "//merror:types",
    ],
)

cc_test(
    name = "expand_expr_test",
    size = "small",
    srcs = ["expand_expr_test.cc"],
    deps = [
        ":expand_expr",
        "//merror:types",
        "@googletest//:gtest_main",
   ],
)

cc_library(
    name = "preprocessor",
    hdrs = ["preprocessor.h"],
    deps = [
    ],
)

cc_test(
    name = "preprocessor_test",
    size = "small",
    srcs = ["preprocessor_test.cc"],
    deps = [
        ":preprocessor",
        "@googletest//:gtest_main",
    ],
)
