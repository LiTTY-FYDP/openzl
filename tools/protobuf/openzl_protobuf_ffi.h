#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenZLProtobufContext OpenZLProtobufContext;

typedef enum OpenZLProtobufSchemaType {
    OPENZL_PROTOBUF_SCHEMA_PROTO = 0,
    OPENZL_PROTOBUF_SCHEMA_DESCRIPTOR = 1,
} OpenZLProtobufSchemaType;

typedef struct OpenZLProtobufSchema {
    OpenZLProtobufSchemaType schema_type;
    const char* schema_path;
    const char* message_type;
    const char* const* proto_paths;
    size_t proto_paths_len;
} OpenZLProtobufSchema;

typedef struct OpenZLBuffer {
    uint8_t* data;
    size_t len;
} OpenZLBuffer;

typedef enum OpenZLProtobufClusteringTrainer {
    OPENZL_PROTOBUF_CLUSTERING_TRAINER_GREEDY = 0,
    OPENZL_PROTOBUF_CLUSTERING_TRAINER_BOTTOM_UP = 1,
    OPENZL_PROTOBUF_CLUSTERING_TRAINER_FULL_SPLIT = 2,
} OpenZLProtobufClusteringTrainer;

typedef struct OpenZLProtobufTrainParams {
    uint8_t has_threads;
    uint32_t threads;
    uint8_t has_clustering_trainer;
    OpenZLProtobufClusteringTrainer clustering_trainer;
    uint8_t has_max_time_secs;
    size_t max_time_secs;
    uint8_t has_no_ace_successors;
    uint8_t no_ace_successors;
    uint8_t has_no_clustering;
    uint8_t no_clustering;
} OpenZLProtobufTrainParams;

typedef struct OpenZLProtobufParetoResult {
    size_t index;
    double compression_ratio;
    double compression_speed;
    double decompression_speed;
} OpenZLProtobufParetoResult;

OpenZLProtobufContext* openzl_protobuf_create(
        const OpenZLProtobufSchema* schema);
void openzl_protobuf_destroy(OpenZLProtobufContext* ctx);
const char* openzl_protobuf_last_error(const OpenZLProtobufContext* ctx);

int openzl_protobuf_set_compressor(
        OpenZLProtobufContext* ctx,
        const uint8_t* compressor_data,
        size_t compressor_len);
int openzl_protobuf_compress(
        OpenZLProtobufContext* ctx,
        const uint8_t* proto_data,
        size_t proto_len,
        OpenZLBuffer* out);
int openzl_protobuf_decompress(
        OpenZLProtobufContext* ctx,
        const uint8_t* zl_data,
        size_t zl_len,
        OpenZLBuffer* out);
int openzl_protobuf_train(
        OpenZLProtobufContext* ctx,
        const OpenZLBuffer* samples,
        size_t samples_len,
        OpenZLBuffer* out_compressor);
int openzl_protobuf_train_with_params(
        OpenZLProtobufContext* ctx,
        const OpenZLBuffer* samples,
        size_t samples_len,
        const OpenZLProtobufTrainParams* params,
        OpenZLBuffer* out_compressor);
int openzl_protobuf_train_pareto(
        OpenZLProtobufContext* ctx,
        const OpenZLBuffer* samples,
        size_t samples_len,
        const OpenZLProtobufTrainParams* params,
        OpenZLProtobufParetoResult** out_results,
        size_t* out_results_len);

void openzl_protobuf_free_buffer(OpenZLBuffer* buffer);
void openzl_protobuf_free_pareto_results(OpenZLProtobufParetoResult* results);

#ifdef __cplusplus
}
#endif
