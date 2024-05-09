load('@bazel_tools//tools/build_defs/repo:git.bzl', "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

# Sanitize a dependency so that it works correctly from code that includes
# TensorFlow as a submodule.
def clean_dep(dep):
    return str(Label(dep))

def git_deps():
    git_repository(
        name = "rules_python",
        remote = "https://github.com/bazelbuild/rules_python.git",
        patches = ["//patches/rules_python:0001-fix-triton-and-pypi-wheel.patch"],
        commit = "5eb0de810f76f16ab8a909953c1b235051536686",
        shallow_since = "1611475624 +1100",
    )

    new_git_repository(
        name = "cutlass",
        remote = "https://github.com/NVIDIA/cutlass.git",
        commit = "8783c41851cd3582490e04e69e0cd756a8c1db7f",
        build_file = str(Label("//3rdparty/cutlass:cutlass.BUILD")),
    )

    git_repository(
        name = "com_google_googletest",
        remote = "https://github.com/google/googletest.git",
        commit = "f8d7d77c06936315286eb55f8de22cd23c188571",
        shallow_since = "1640057570 +0800",
    )

    http_archive(
        name = "com_github_nanopb_nanopb",
        sha256 = "8bbbb1e78d4ddb0a1919276924ab10d11b631df48b657d960e0c795a25515735",
        build_file = "@grpc//third_party:nanopb.BUILD",
        strip_prefix = "nanopb-f8ac463766281625ad710900479130c7fcb4d63b",
        urls = [
            "http://storage.googleapis.com/mirror.tensorflow.org/github.com/nanopb/nanopb/archive/f8ac463766281625ad710900479130c7fcb4d63b.tar.gz",
            "http://github.com/nanopb/nanopb/archive/f8ac463766281625ad710900479130c7fcb4d63b.tar.gz",
        ],
    )

    http_archive(
        name = "six_archive",
        build_file = clean_dep("//third_party:six.BUILD"),
        sha256 = "105f8d68616f8248e24bf0e9372ef04d3cc10104f1980f54d57b2ce73a5ad56a",
        strip_prefix = "six-1.10.0",
        urls = [
            "http://mirror.bazel.build/pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz",
            "http://pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz",
        ],
    )

    http_archive(
        name = "zlib_archive",
        build_file = clean_dep("//third_party:zlib.BUILD"),
        strip_prefix = "zlib-1.2.11",
        urls = [
            "https://www.zlib.net/fossils/zlib-1.2.11.tar.gz",
        ],
        sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    )

    git_repository(
        name = "com_google_absl",
        remote = "https://github.com/abseil/abseil-cpp.git",
        patch_cmds = ["sed -i -e 's/^#define ABSL_OPTION_USE_STD_STRING_VIEW 2/#define ABSL_OPTION_USE_STD_STRING_VIEW 0/' 'absl/base/options.h'"],
        commit = "6f9d96a1f41439ac172ee2ef7ccd8edf0e5d068c",
        shallow_since = "1678195250 +0800",
    )

    git_repository(
        name = "com_google_protobuf",
        remote = "git@github.com:protocolbuffers/protobuf.git",
        # tag = 3.7
        commit = "a2a0afb5468dc423782344a2047abc041e75323e",
        shallow_since = "1518192000 +0800",
        # build_file = str(Label("//3rdparty/protobuf:protobuf.BUILD")),
    )

    git_repository(
        name = "grpc",
        remote = "https://github.com/grpc/grpc.git",
        commit = "109c570727c3089fef655edcdd0dd02cc5958010",
        patches = ["//patches/grpc:0001-Rename-gettid-functions.patch"],
        shallow_since = "1518192000 +0800",
    )

    http_file(
        name = "openssl-devel",
        urls = ["http://mirrors.aliyun.com/centos/7/os/x86_64/Packages/openssl-devel-1.0.2k-19.el7.x86_64.rpm"],
        sha256 = "5724c9c1274fbfdf415cfc46aee1b67d281699d22087e0dcc1309400c077e45a",
    )

    http_file(
        name = "openssl",
        urls = ["http://mirrors.aliyun.com/centos/7/os/x86_64/Packages/openssl-1.0.2k-19.el7.x86_64.rpm"],
        sha256 = "f568b3dd3aa069c7d0419055de478903ca61dd033677880882f0e22ac6ed1214",
    )

    http_file(
        name = "openssl-libs",
        urls = ["http://mirrors.aliyun.com/centos/7/os/x86_64/Packages/openssl-libs-1.0.2k-19.el7.x86_64.rpm"],
        sha256 = "ee1a517bfc1856eed2622e278bc42bedd1fb91aac76bef74a8b7eb91e42869d9",
    )

    http_file(
        name = "krb5-devel",
        urls = ["http://mirrors.aliyun.com/centos/7/os/x86_64/Packages/krb5-devel-1.15.1-50.el7.x86_64.rpm"],
        sha256 = "a0455890cde800204879f358de750c322fd174d62ffc89a8046309d6fcf0e3a2",
    )

    http_file(
        name = "libcom_err-devel",
        urls = ["https://mirrors.aliyun.com/centos/7/os/x86_64/Packages/libcom_err-devel-1.42.9-19.el7.x86_64.rpm"],
        sha256 = "d6ed17613d58bdc7623097ae40e847a57f2900d256a8668694296948c12c69af",
    )

    # Needed by Protobuf
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@grpc//:grpc_cpp_plugin",
    )

    native.bind(
        name = "grpc_python_plugin",
        actual = "@grpc//:grpc_python_plugin",
    )

    # Needed by gRPC
    native.bind(
        name = "libssl",
        actual = "//3rdparty/openssl:ssl",
    )

    # Needed by gRPC
    native.bind(
        name = "nanopb",
        actual = "@com_github_nanopb_nanopb//:nanopb",
    )

    # gRPC expects //external:protobuf_clib and //external:protobuf_compiler
    # to point to Protobuf's compiler library.
    native.bind(
        name = "protobuf_clib",
        actual = "@com_google_protobuf//:protoc_lib",
    )

    # Needed by gRPC
    native.bind(
        name = "protobuf_headers",
        actual = "@com_google_protobuf//:protobuf_headers",
    )

    # # Needed by Protobuf
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@grpc//:grpc_cpp_plugin",
    )
    native.bind(
        name = "grpc_python_plugin",
        actual = "@grpc//:grpc_python_plugin",
    )

    # # Needed by Protobuf
    native.bind(
        name = "six",
        actual = "@six_archive//:six",
    )

    # Needed by gRPC
    native.bind(
        name = "zlib",
        actual = "@zlib_archive//:zlib",
    )
