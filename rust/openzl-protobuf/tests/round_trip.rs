use std::path::PathBuf;

use openzl_protobuf::{OpenZLProtobuf, Schema};
use prost::Message;

#[derive(Clone, PartialEq, Message)]
struct TestMessage {
    #[prost(uint32, tag = "1")]
    id: u32,
    #[prost(string, tag = "2")]
    name: String,
    #[prost(int64, repeated, tag = "3")]
    values: Vec<i64>,
}

fn schema() -> Schema {
    let proto_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("proto");
    Schema::proto(
        "test_message.proto",
        "openzl.test.TestMessage",
        vec![proto_dir.to_string_lossy().to_string()],
    )
}

fn sample_message() -> TestMessage {
    TestMessage {
        id: 42,
        name: "openzl".to_string(),
        values: vec![1, -2, 3, 4, -5],
    }
}

#[test]
fn round_trip_protobuf_message() -> Result<(), Box<dyn std::error::Error>> {
    let zl = OpenZLProtobuf::new(schema())?;
    let message = sample_message();
    let original = message.encode_to_vec();

    let compressed = zl.compress(&original)?;
    let decompressed = zl.decompress(&compressed)?;
    let decoded = TestMessage::decode(decompressed.as_slice())?;

    assert_eq!(decoded, message);
    Ok(())
}

#[test]
#[cfg(feature = "training")]
fn train_compressor_round_trip() -> Result<(), Box<dyn std::error::Error>> {
    let mut zl = OpenZLProtobuf::new(schema())?;
    let message = sample_message();
    let sample = message.encode_to_vec();
    let samples = [&sample[..], &sample[..]];

    let compressor = zl.train_compressor(&samples)?;
    zl.set_compressor(&compressor)?;

    let compressed = zl.compress(&sample)?;
    let decompressed = zl.decompress(&compressed)?;
    let decoded = TestMessage::decode(decompressed.as_slice())?;

    assert_eq!(decoded, message);
    Ok(())
}
