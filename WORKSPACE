load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_rust",
    integrity = "sha256-Weev1uz2QztBlDA88JX6A1N72SucD1V8lBsaliM0TTg=",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.48.0/rules_rust-v0.48.0.tar.gz"],
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")


rules_rust_dependencies()
rust_register_toolchains(
    edition = "2021",
    versions = ["1.76.0", "nightly/2024-02-01"],
    sha256s = {
        "2024-02-01/rustc-nightly-x86_64-unknown-linux-gnu.tar.xz": "7247ca497c7d9194c9e7bb9b6a51f8ccddc452bbce2977d608cabdbc1a0f332f",
        "2024-02-01/clippy-nightly-x86_64-unknown-linux-gnu.tar.xz": "1271eaa89d50bd7f63b338616c36f41fe1e733b5d6c4cc2c95eaa6b3c8faba62",
        "2024-02-01/cargo-nightly-x86_64-unknown-linux-gnu.tar.xz": "1d859549b5f3d2dd146b84aa13dfec24a05913653af2116b39a919cab69de850",
        "2024-02-01/llvm-tools-nightly-x86_64-unknown-linux-gnu.tar.xz": "b227753189981d9a115527ba0e95b365388fb0fe7f1a1ff93116c4448c854197",
        "2024-02-01/rust-std-nightly-x86_64-unknown-linux-gnu.tar.xz": "b1a444f8e8f33d813c4d532c12717743edd9b34f685ff5293b6375fc75c2421e",
    },
)
# Additional setup for bindgen
load("@rules_rust//bindgen:repositories.bzl", "rust_bindgen_dependencies", "rust_bindgen_register_toolchains")
rust_bindgen_dependencies()
rust_bindgen_register_toolchains()
load("@rules_rust//bindgen:transitive_repositories.bzl", "rust_bindgen_transitive_dependencies")
rust_bindgen_transitive_dependencies()
