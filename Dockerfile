FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG OPENZL_BUILD_JOBS

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        git \
        ninja-build \
        protobuf-compiler \
        libprotobuf-dev \
        libprotoc-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

ADD . .

# Build the OpenZL tools, including protobuf tooling, with tests and
# introspection disabled. Force PIC so the protobuf FFI shared library can
# link against the fetched static protobuf libraries.
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DOPENZL_BUILD_TOOLS=ON \
        -DOPENZL_BUILD_PROTOBUF_TOOLS=ON \
        -DOPENZL_BUILD_LOGGER=ON \
        -DOPENZL_BUILD_TESTS=OFF \
        -DOPENZL_ALLOW_INTROSPECTION=OFF \
        -DOPENZL_BUILD_BENCHMARKS=OFF \
        -DOPENZL_BUILD_CLI=OFF \
        -DOPENZL_BUILD_EXAMPLES=OFF \
        -DOPENZL_BUILD_PARQUET_TOOLS=OFF \
        -DOPENZL_BUILD_PYTHON_EXT=OFF \
        -DOPENZL_BUILD_PYTHON_DEMO=OFF \
    && build_jobs="${OPENZL_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN)}" \
    && cmake --build build --parallel "${build_jobs}"

CMD ["/bin/bash"]
