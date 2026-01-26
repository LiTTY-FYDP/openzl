use std::path::PathBuf;

use openzl_protobuf::{ClusteringTrainer, OpenZLProtobuf, ParetoCompressor, Schema, TrainParams};
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
        id: 99,
        name: "pareto".to_string(),
        values: vec![4, -8, 15, -16, 23, 42],
    }
}

fn assert_sorted_by_ratio(results: &[ParetoCompressor]) {
    for window in results.windows(2) {
        assert!(
            window[0].compression_ratio >= window[1].compression_ratio,
            "results are not sorted by compression ratio descending"
        );
    }
}

#[test]
fn train_pareto_frontier_metrics() -> Result<(), Box<dyn std::error::Error>> {
    let zl = OpenZLProtobuf::new(schema())?;
    let message = sample_message();
    let sample = message.encode_to_vec();
    let samples = [&sample[..], &sample[..], &sample[..]];

    let params = TrainParams {
        threads: None,
        clustering_trainer: Some(ClusteringTrainer::FullSplit),
        max_time_secs: Some(2),
        no_ace_successors: true,
        no_clustering: false,
    };

    let results = zl.train_pareto_frontier(&samples, params)?;
    assert!(!results.is_empty(), "pareto frontier results are empty");
    assert_sorted_by_ratio(&results);

    for entry in &results {
        println!(
            "index={} ratio={:.4} compress_mb_s={:.2} decompress_mb_s={:.2}",
            entry.index,
            entry.compression_ratio,
            entry.compression_speed,
            entry.decompression_speed
        );
        assert!(entry.compression_ratio > 0.0);
        assert!(entry.compression_speed > 0.0);
        assert!(entry.decompression_speed > 0.0);
    }

    Ok(())
}
