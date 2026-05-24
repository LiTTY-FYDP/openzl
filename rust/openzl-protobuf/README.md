# OpenZL Protobuf Rust Bindings

This crate provides Rust bindings for OpenZL's protobuf compression,
decompression, and compressor training APIs. The bindings operate on standard
protobuf wire format bytes and support dynamic schema loading via `.proto` or
compiled descriptor (`.desc`) files.

## Requirements

- CMake and a C++17 compiler
- Protobuf (fetched automatically when building OpenZL)
- OpenZL dependencies (zstd, lz4, and xgboost via the OpenZL build)

By default, schemas load from compiled descriptor sets. Enable the
`proto-source` feature to parse `.proto` source files at runtime.

## Example

```rust
use openzl_protobuf::{OpenZLProtobuf, Schema};

let schema = Schema::descriptor(
    "./schema.desc",
    "mypackage.MyMessage",
);
let zl = OpenZLProtobuf::new(schema)?;

let compressed = zl.compress(&sample_a)?;
let round_trip = zl.decompress(&compressed)?;
```
