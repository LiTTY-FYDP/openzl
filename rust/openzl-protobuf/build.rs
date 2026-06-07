use std::env;
use std::fs::read_to_string;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};

fn command_succeeds(command: &str) -> bool {
    Command::new(command)
        .arg("--version")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .is_ok_and(|status| status.success())
}

fn find_ninja_command() -> Option<&'static str> {
    ["ninja", "ninja-build"]
        .into_iter()
        .find(|command| command_succeeds(command))
}

fn existing_cmake_generator(out_dir: &Path) -> Option<String> {
    let cache_path = out_dir.join("build").join("CMakeCache.txt");
    let contents = read_to_string(cache_path).ok()?;
    contents.lines().find_map(|line| {
        line.strip_prefix("CMAKE_GENERATOR:INTERNAL=")
            .map(ToOwned::to_owned)
    })
}

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("missing manifest"));
    let root_dir = manifest_dir
        .join("../..")
        .canonicalize()
        .expect("failed to resolve repo root");
    let proto_source_enabled = env::var_os("CARGO_FEATURE_PROTO_SOURCE").is_some();
    let training_enabled = env::var_os("CARGO_FEATURE_TRAINING").is_some();

    println!("cargo:rerun-if-env-changed=CMAKE_GENERATOR");
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
            .join("tools/protobuf/DescriptorLoader.cpp")
            .display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        root_dir.join("tools/protobuf/DescriptorLoader.h").display()
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
            "OPENZL_BUILD_PROTOBUF_PROTO_SOURCE",
            if proto_source_enabled { "ON" } else { "OFF" },
        )
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
    if env::var_os("CMAKE_GENERATOR").is_none() && existing_cmake_generator(&out_dir).is_none() {
        if let Some(ninja_command) = find_ninja_command() {
            cmake_config.generator("Ninja");
            if ninja_command != "ninja" {
                cmake_config.define("CMAKE_MAKE_PROGRAM", ninja_command);
            }
        }
    }
    if let Ok(parallelism) = std::thread::available_parallelism() {
        cmake_config.env("CMAKE_BUILD_PARALLEL_LEVEL", parallelism.get().to_string());
    }
    cmake_config.build();

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=dylib=openzl_protobuf_core_ffi");
    if training_enabled {
        println!("cargo:rustc-link-lib=dylib=openzl_protobuf_train_ffi");
    }

    // Expose the native lib directory to direct dependents (via DEP_OPENZL_PROTOBUF_CORE_FFI_LIB_DIR,
    // because this crate sets `links`). A build script's `rustc-link-arg` rpath below applies only to
    // this crate's own targets and does NOT propagate to a dependent binary, so dependents must bake
    // the rpath themselves using this path to find the FFI .so at runtime without LD_LIBRARY_PATH.
    println!("cargo:lib_dir={}", lib_dir.display());

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
