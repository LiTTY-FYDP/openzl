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
    let descriptor_path =
        std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("proto/test_message.desc.bin");
    Schema::descriptor(
        descriptor_path.to_string_lossy().to_string(),
        "openzl.test.TestMessage",
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
        ace_successors: false,
        clustering: true,
    };

    let compressor = zl.train_compressor_with_params(&samples, params)?;
    zl.set_compressor(&compressor)?;

    let compressed = zl.compress(&sample)?;
    let decompressed = zl.decompress(&compressed)?;
    let decoded = TestMessage::decode(decompressed.as_slice())?;

    assert_eq!(decoded, message);
    Ok(())
}
