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
    let descriptor_path =
        std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("proto/test_message.desc.bin");
    Schema::descriptor(
        descriptor_path.to_string_lossy().to_string(),
        "openzl.test.TestMessage",
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
#[cfg(feature = "proto-source")]
fn round_trip_protobuf_message_from_proto_source() -> Result<(), Box<dyn std::error::Error>> {
    let proto_dir = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("proto");
    let zl = OpenZLProtobuf::new(Schema::proto(
        "test_message.proto",
        "openzl.test.TestMessage",
        vec![proto_dir.to_string_lossy().to_string()],
    ))?;
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
