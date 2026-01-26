use std::path::PathBuf;

use openzl_protobuf::{ClusteringTrainer, OpenZLProtobuf, Schema, TrainParams};
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
        id: 7,
        name: "training".to_string(),
        values: vec![10, -20, 30],
    }
}

#[test]
fn train_with_options_round_trip() -> Result<(), Box<dyn std::error::Error>> {
    let mut zl = OpenZLProtobuf::new(schema())?;
    let message = sample_message();
    let sample = message.encode_to_vec();
    let samples = [&sample[..], &sample[..]];

    let params = TrainParams {
        threads: Some(1),
        clustering_trainer: Some(ClusteringTrainer::Greedy),
        max_time_secs: Some(1),
        no_ace_successors: true,
        no_clustering: false,
    };

    let compressor = zl.train_compressor_with_params(&samples, params)?;
    zl.set_compressor(&compressor)?;

    let compressed = zl.compress(&sample)?;
    let decompressed = zl.decompress(&compressed)?;
    let decoded = TestMessage::decode(decompressed.as_slice())?;

    assert_eq!(decoded, message);
    Ok(())
}
