# Building OpenZL

OpenZL uses CMake as its build system.

## Prerequisites

### General
*   **C++ Compiler**: C++17 compliant compiler.
    *   GCC 12 or later (Recommended)
    *   Clang 18 or later
*   **CMake**: Version 3.20 or later.
*   **Git**: For fetching dependencies.

### Tools Specific
To build the `protobuf_cli` tool, you need Protobuf development libraries installed:

*   **Protobuf Compiler (`protoc`)**
*   **Protobuf Development Libraries (`libprotobuf-dev`, `libprotoc-dev`)**

#### Installing Prerequisites on Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git
# For protobuf_cli
sudo apt-get install -y protobuf-compiler libprotobuf-dev libprotoc-dev
```

## Building

1.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```

2.  Configure the build with CMake:
    ```bash
    cmake ..
    ```

    **Options:**
    *   `-DOPENZL_BUILD_TOOLS=ON`: Build tools (enabled by default).
    *   `-DOPENZL_BUILD_PROTOBUF_TOOLS=ON`: Build Protobuf tools (requires protobuf deps).
    *   `-DOPENZL_BUILD_TESTS=ON`: Build tests.
    *   `-DOPENZL_ALLOW_INTROSPECTION=OFF`: Disable introspection (recommended for release/performance).

    Example to build everything including protobuf tools:
    ```bash
    cmake .. -DOPENZL_BUILD_PROTOBUF_TOOLS=ON
    ```

3.  Build:
    ```bash
    make -j $(sysctl -n hw.logicalcpu)
    make -j $(sysctl -n hw.logicalcpu) protobuf_cli
    ```

## Running Tests

If you built with tests enabled:
```bash
ctest --output-on-failure
```
