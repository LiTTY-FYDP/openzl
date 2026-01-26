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

void openzl_protobuf_free_buffer(OpenZLBuffer* buffer);

#ifdef __cplusplus
}
#endif
