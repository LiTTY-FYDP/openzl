# OpenZL Protobuf Rust Bindings

This crate provides Rust bindings for OpenZL's protobuf compression,
decompression, and compressor training APIs. The bindings operate on standard
protobuf wire format bytes and support dynamic schema loading via `.proto` or
compiled descriptor (`.desc`) files.

## Requirements

- CMake and a C++17 compiler
- Protobuf (fetched automatically when building OpenZL)
- OpenZL dependencies (zstd, lz4, and xgboost via the OpenZL build)

## Example

```rust
use openzl_protobuf::{OpenZLProtobuf, Schema};

let schema = Schema::proto(
    "schema.proto",
    "mypackage.MyMessage",
    vec!["./proto".to_string()],
);
let mut zl = OpenZLProtobuf::new(schema)?;

let compressor = zl.train_compressor(&[&sample_a, &sample_b])?;
zl.set_compressor(&compressor)?;

let compressed = zl.compress(&sample_a)?;
let round_trip = zl.decompress(&compressed)?;
```
