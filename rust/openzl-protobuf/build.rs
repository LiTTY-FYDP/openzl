use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("missing manifest"));
    let root_dir = manifest_dir
        .join("../..")
        .canonicalize()
        .expect("failed to resolve repo root");
    let training_enabled = env::var_os("CARGO_FEATURE_TRAINING").is_some();

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
            .join("tools/protobuf/openzl_protobuf_core_ffi.cpp")
            .display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir
            .join("tools/protobuf/openzl_protobuf_ffi_internal.h")
            .display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir
            .join("tools/protobuf/openzl_protobuf_ffi.h")
            .display()
    );
    if training_enabled {
        println!(
            "cargo:rerun-if-changed={}",
            root_dir
                .join("tools/protobuf/openzl_protobuf_train_ffi.cpp")
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
    }

    let profile = env::var("PROFILE").unwrap_or_else(|_| "release".to_string());
    let build_type = if profile == "release" {
        "Release"
    } else {
        "Debug"
    };
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("missing OUT_DIR"));
    let lib_dir = out_dir.join("lib");

    let mut cmake_config = cmake::Config::new(&root_dir);
    cmake_config
        .profile(build_type)
        .define("OPENZL_BUILD_PROTOBUF_TOOLS", "ON")
        .define("OPENZL_BUILD_PROTOBUF_CLI", "OFF")
        .define(
            "OPENZL_BUILD_PROTOBUF_TRAINING",
            if training_enabled { "ON" } else { "OFF" },
        )
        .define("OPENZL_BUILD_BENCHMARKS", "OFF")
        .define("OPENZL_BUILD_TESTS", "OFF")
        .define("OPENZL_BUILD_TOOLS", "OFF")
        .define("OPENZL_BUILD_CLI", "OFF")
        .define(
            "OPENZL_BUILD_CUSTOM_PARSERS",
            if training_enabled { "ON" } else { "OFF" },
        )
        .define(
            "OPENZL_BUILD_LOGGER",
            if training_enabled { "ON" } else { "OFF" },
        )
        .define("OPENZL_BUILD_EXAMPLES", "OFF")
        .define("OPENZL_BUILD_PYTHON_EXT", "OFF")
        .define("OPENZL_BUILD_PYTHON_EXT_TESTS", "OFF")
        .define("OPENZL_BUILD_PYTHON_DEMO", "OFF")
        .define("OPENZL_INSTALL", "OFF")
        .define("OPENZL_CPP_INSTALL", "ON")
        .define("CMAKE_POSITION_INDEPENDENT_CODE", "ON")
        .define("CMAKE_LIBRARY_OUTPUT_DIRECTORY", &lib_dir)
        .define("CMAKE_ARCHIVE_OUTPUT_DIRECTORY", &lib_dir)
        .define("CMAKE_RUNTIME_OUTPUT_DIRECTORY", &lib_dir)
        .build_target(if training_enabled {
            "openzl_protobuf_train"
        } else {
            "openzl_protobuf_core"
        });
    if let Ok(parallelism) = std::thread::available_parallelism() {
        cmake_config.env("CMAKE_BUILD_PARALLEL_LEVEL", parallelism.get().to_string());
    }
    cmake_config.build();

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=dylib=openzl_protobuf_core_ffi");
    if training_enabled {
        println!("cargo:rustc-link-lib=dylib=openzl_protobuf_train_ffi");
    }

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
