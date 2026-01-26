use std::env;
use std::path::PathBuf;
use std::process::Command;

fn run_command(cmd: &mut Command) {
    let status = cmd.status().expect("failed to spawn command");
    if !status.success() {
        panic!("command failed with status: {}", status);
    }
}

fn main() {
    let manifest_dir =
        PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("missing manifest"));
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

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("missing OUT_DIR"));
    let build_dir = out_dir.join("openzl-build");
    let install_dir = out_dir.join("openzl-install");

    let profile = env::var("PROFILE").unwrap_or_else(|_| "release".to_string());
    let build_type = if profile == "release" {
        "Release"
    } else {
        "Debug"
    };

    let mut cmake_config = Command::new("cmake");
    cmake_config
        .arg("-S")
        .arg(&root_dir)
        .arg("-B")
        .arg(&build_dir)
        .arg(format!("-DCMAKE_BUILD_TYPE={}", build_type))
        .arg("-DOPENZL_BUILD_PROTOBUF_TOOLS=ON")
        .arg("-DOPENZL_BUILD_BENCHMARKS=OFF")
        .arg("-DOPENZL_BUILD_TESTS=OFF")
        .arg("-DOPENZL_BUILD_CLI=OFF")
        .arg("-DOPENZL_BUILD_LOGGER=ON")
        .arg("-DOPENZL_BUILD_EXAMPLES=OFF")
        .arg("-DOPENZL_BUILD_PYTHON_EXT=OFF")
        .arg("-DOPENZL_BUILD_PYTHON_EXT_TESTS=OFF")
        .arg("-DOPENZL_BUILD_PYTHON_DEMO=OFF")
        .arg("-DOPENZL_INSTALL=ON")
        .arg("-DOPENZL_CPP_INSTALL=ON")
        .arg("-DCMAKE_POSITION_INDEPENDENT_CODE=ON")
        .arg(format!(
            "-DCMAKE_INSTALL_PREFIX={}",
            install_dir.display()
        ));
    run_command(&mut cmake_config);

    let mut cmake_build = Command::new("cmake");
    cmake_build
        .arg("--build")
        .arg(&build_dir)
        .arg("--target")
        .arg("install");
    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("windows") {
        cmake_build.arg("--config").arg(build_type);
    }
    run_command(&mut cmake_build);

    let lib_dir = install_dir.join("lib");
    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=dylib=openzl_protobuf_ffi");

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("linux") {
        println!(
            "cargo:rustc-link-arg=-Wl,-rpath,{}",
            lib_dir.display()
        );
    } else if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("macos") {
        println!(
            "cargo:rustc-link-arg=-Wl,-rpath,{}",
            lib_dir.display()
        );
    }

    if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("linux") {
        println!("cargo:rustc-link-lib=stdc++");
    } else if env::var("CARGO_CFG_TARGET_OS").as_deref() == Ok("macos") {
        println!("cargo:rustc-link-lib=c++");
    }
}
