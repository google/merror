package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "default",
    hdrs = ["default.h"],
    deps = [
        ":adl_hooks",
        ":base",
        ":bool",
        ":description",
        ":error_passthrough",
        ":fill_error",
        ":forward",
        ":function",
        ":logging",
        ":method_hooks",
        ":optional",
        ":pointer",
        ":print",
        ":print_operands",
        ":return",
        ":status",
        ":tee",
        ":verify_via_try",
    ],
)

cc_test(
    name = "default_test",
    size = "small",
    srcs = ["default_test.cc"],
    deps = [
        ":default",
        "//merror:macros",
        "@absl//absl/status:statusor",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "base",
    hdrs = ["base.h"],
    deps = [
        ":defer",
        "//merror:types",
        "//merror/domain/internal:type_map",
    ],
)

cc_test(
    name = "base_test",
    size = "small",
    srcs = ["base_test.cc"],
    deps = [
        ":base",
        ":defer",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "defer",
    hdrs = ["defer.h"],
    deps = [
    ],
)

cc_test(
    name = "defer_test",
    size = "small",
    srcs = ["defer_test.cc"],
    deps = [
        ":defer",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "observer",
    hdrs = ["observer.h"],
    deps = [
    ],
)

cc_test(
    name = "observer_test",
    size = "small",
    srcs = ["observer_test.cc"],
    deps = [
        ":base",
        ":observer",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "return",
    hdrs = ["return.h"],
    deps = [
        ":base",
        ":observer",
        "//merror:types",
    ],
)

cc_test(
    name = "return_test",
    size = "small",
    srcs = ["return_test.cc"],
    deps = [
        ":base",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "method_hooks",
    hdrs = ["method_hooks.h"],
    deps = [
        ":base",
        ":defer",
        ":return",
    ],
)

cc_test(
    name = "method_hooks_test",
    size = "small",
    srcs = ["method_hooks_test.cc"],
    deps = [
        ":base",
        ":defer",
        ":method_hooks",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "fill_error",
    hdrs = ["fill_error.h"],
    deps = [
        ":base",
        ":return",
    ],
)

cc_test(
    name = "fill_error_test",
    size = "small",
    srcs = ["fill_error_test.cc"],
    deps = [
        ":base",
        ":bool",
        ":fill_error",
        ":method_hooks",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "description",
    hdrs = ["description.h"],
    deps = [
        ":base",
        ":defer",
        "//merror/domain/internal:stringstream",
    ],
)

cc_test(
    name = "description_test",
    size = "small",
    srcs = ["description_test.cc"],
    deps = [
        ":base",
        ":defer",
        ":description",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "status",
    hdrs = ["status.h"],
    deps = [
        ":defer",
        ":description",
        ":method_hooks",
        ":print",
        ":return",
        "//merror/domain/internal:indenting_stream",
        "@absl//absl/status",
        "@absl//absl/status:statusor",
        "@absl//absl/strings",
    ],
)

cc_test(
    name = "status_test",
    size = "small",
    srcs = ["status_test.cc"],
    deps = [
        ":bool",
        ":description",
        ":method_hooks",
        ":print_operands",
        ":return",
        ":status",
        "//merror:macros",
        "@absl//absl/status",
        "@absl//absl/status:statusor",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "bool",
    hdrs = ["bool.h"],
    deps = [
        ":base",
        ":method_hooks",
        ":return",
    ],
)

cc_test(
    name = "bool_test",
    size = "small",
    srcs = ["bool_test.cc"],
    deps = [
        ":base",
        ":bool",
        ":method_hooks",
        ":observer",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "pointer",
    hdrs = ["pointer.h"],
    deps = [
        ":base",
        ":method_hooks",
        ":return",
    ],
)

cc_test(
    name = "pointer_test",
    size = "small",
    srcs = ["pointer_test.cc"],
    deps = [
        ":pointer",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "optional",
    hdrs = ["optional.h"],
    deps = [
        ":base",
        ":method_hooks",
        ":return",
    ],
)

cc_test(
    name = "optional_test",
    size = "small",
    srcs = ["optional_test.cc"],
    deps = [
        ":optional",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "print",
    hdrs = ["print.h"],
    deps = [
        ":base",
    ],
)

cc_test(
    name = "print_test",
    size = "small",
    srcs = ["print_test.cc"],
    deps = [
        ":base",
        ":method_hooks",
        ":print",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "print_operands",
    hdrs = ["print_operands.h"],
    deps = [
        ":base",
        ":print",
        "//merror/domain/internal:stringstream",
    ],
)

cc_test(
    name = "print_operands_test",
    size = "small",
    srcs = ["print_operands_test.cc"],
    deps = [
        ":base",
        ":bool",
        ":method_hooks",
        ":print_operands",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "error_passthrough",
    hdrs = ["error_passthrough.h"],
    deps = [
        ":base",
        ":return",
    ],
)

cc_test(
    name = "error_passthrough_test",
    size = "small",
    srcs = ["error_passthrough_test.cc"],
    deps = [
        ":base",
        ":defer",
        ":error_passthrough",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "verify_via_try",
    hdrs = ["verify_via_try.h"],
    deps = [
        ":base",
    ],
)

cc_test(
    name = "verify_via_try_test",
    size = "small",
    srcs = ["verify_via_try_test.cc"],
    deps = [
        ":verify_via_try",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "forward",
    hdrs = ["forward.h"],
    deps = [
        ":base",
        ":defer",
    ],
)

cc_test(
    name = "forward_test",
    size = "small",
    srcs = ["forward_test.cc"],
    deps = [
        ":base",
        ":forward",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tee",
    hdrs = ["tee.h"],
    deps = [
        ":base",
        ":defer",
        ":observer",
        ":return",
        "//merror:types",
    ],
)

cc_test(
    name = "tee_test",
    size = "small",
    srcs = ["tee_test.cc"],
    deps = [
        ":base",
        ":bool",
        ":fill_error",
        ":method_hooks",
        ":return",
        ":tee",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "adl_hooks",
    hdrs = ["adl_hooks.h"],
    deps = [
        ":base",
        ":defer",
        ":return",
    ],
)

cc_test(
    name = "adl_hooks_test",
    size = "small",
    srcs = ["adl_hooks_test.cc"],
    deps = [
        ":adl_hooks",
        ":base",
        ":defer",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "logging",
    srcs = ["logging.cc"],
    hdrs = ["logging.h"],
    deps = [
        ":base",
        ":defer",
        ":description",
        ":observer",
        ":print",
        "//merror/domain/internal:indenting_stream",
    ],
)

cc_test(
    name = "logging_test",
    size = "small",
    srcs = ["logging_test.cc"],
    deps = [
        ":base",
        ":bool",
        ":description",
        ":logging",
        ":method_hooks",
        ":print",
        ":print_operands",
        ":return",
        ":status",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "function",
    hdrs = ["function.h"],
    deps = [
        ":base",
        ":method_hooks",
        ":return",
    ],
)

cc_test(
    name = "function_test",
    size = "small",
    srcs = ["function_test.cc"],
    deps = [
        ":base",
        ":function",
        ":method_hooks",
        ":observer",
        ":return",
        "//merror:macros",
        "@googletest//:gtest_main",
    ],
)
