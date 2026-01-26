use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("missing manifest"));
    let root_dir = manifest_dir
        .join("../..")
        .canonicalize()
        .expect("failed to resolve repo root");

    println!(
        "cargo:rerun-if-changed={}",
        root_dir.join("CMakeLists.txt").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir.join("tools/protobuf/CMakeLists.txt").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir
            .join("tools/protobuf/openzl_protobuf_ffi.cpp")
            .display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir
            .join("tools/protobuf/openzl_protobuf_ffi.h")
            .display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir.join("tools/training/ace/ace.cpp").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir
            .join("tools/training/ace/ace_combination.cpp")
            .display()
    );

    let profile = env::var("PROFILE").unwrap_or_else(|_| "release".to_string());
    let build_type = if profile == "release" {
        "Release"
    } else {
        "Debug"
    };

    let mut cmake_config = cmake::Config::new(&root_dir);
    cmake_config
        .profile(build_type)
        .define("OPENZL_BUILD_PROTOBUF_TOOLS", "ON")
        .define("OPENZL_BUILD_BENCHMARKS", "OFF")
        .define("OPENZL_BUILD_TESTS", "OFF")
        .define("OPENZL_BUILD_CLI", "OFF")
        .define("OPENZL_BUILD_LOGGER", "ON")
        .define("OPENZL_BUILD_EXAMPLES", "OFF")
        .define("OPENZL_BUILD_PYTHON_EXT", "OFF")
        .define("OPENZL_BUILD_PYTHON_EXT_TESTS", "OFF")
        .define("OPENZL_BUILD_PYTHON_DEMO", "OFF")
        .define("OPENZL_INSTALL", "ON")
        .define("OPENZL_CPP_INSTALL", "ON")
        .define("CMAKE_POSITION_INDEPENDENT_CODE", "ON");
    if let Ok(parallelism) = std::thread::available_parallelism() {
        cmake_config.env(
            "CMAKE_BUILD_PARALLEL_LEVEL",
            parallelism.get().to_string(),
        );
    }
    let dst = cmake_config.build();

    let lib_dir = dst.join("lib");
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=dylib=openzl_protobuf_ffi");

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "linux" => {
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir.display());
            println!("cargo:rustc-link-lib=stdc++");
        }
        "macos" => {
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir.display());
            println!("cargo:rustc-link-lib=c++");
        }
        _ => {}
    }
}
