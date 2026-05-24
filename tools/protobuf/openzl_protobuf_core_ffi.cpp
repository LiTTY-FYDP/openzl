#include "tools/protobuf/openzl_protobuf_ffi_internal.h"

#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>

#include "tools/protobuf/DescriptorLoader.h"

namespace {
using openzl::protobuf::DescriptorLoader;
using openzl::protobuf::DynamicMessageHelper;
using openzl::protobuf::ProtoDeserializer;
using openzl::protobuf::ProtoSerializer;
using namespace openzl::protobuf::ffi;

const char* kNullContextError = "OpenZL Protobuf context is null.";

} // namespace

namespace openzl::protobuf::ffi {

void setError(OpenZLProtobufContext* ctx, std::string message)
{
    if (ctx) {
        ctx->last_error = std::move(message);
    }
}

void clearError(OpenZLProtobufContext* ctx)
{
    if (ctx) {
        ctx->last_error.clear();
    }
}

bool ensureReady(OpenZLProtobufContext* ctx)
{
    if (!ctx) {
        return false;
    }
    if (!ctx->ready) {
        setError(ctx, "OpenZL Protobuf context is not initialized.");
        return false;
    }
    return true;
}

bool copyToBuffer(
        OpenZLProtobufContext* ctx,
        const std::string& data,
        OpenZLBuffer* out)
{
    if (!out) {
        setError(ctx, "Output buffer is null.");
        return false;
    }

    out->data = nullptr;
    out->len  = 0;

    if (data.empty()) {
        return true;
    }

    auto* buffer = new (std::nothrow) uint8_t[data.size()];
    if (!buffer) {
        setError(ctx, "Failed to allocate output buffer.");
        return false;
    }

    std::memcpy(buffer, data.data(), data.size());
    out->data = buffer;
    out->len  = data.size();
    return true;
}

} // namespace openzl::protobuf::ffi

namespace {

bool initializeContext(
        OpenZLProtobufContext* ctx,
        const OpenZLProtobufSchema* schema)
{
    if (!schema) {
        setError(ctx, "Schema is null.");
        return false;
    }
    if (!schema->schema_path || !schema->message_type) {
        setError(ctx, "Schema path and message type are required.");
        return false;
    }
    if (schema->proto_paths_len > 0 && !schema->proto_paths) {
        setError(ctx, "Proto paths length is non-zero but paths are null.");
        return false;
    }

    DescriptorLoader loader;
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
    for (size_t i = 0; i < schema->proto_paths_len; ++i) {
        if (!schema->proto_paths[i]) {
            setError(ctx, "Proto path entry is null.");
            return false;
        }
        loader.addProtoPath(schema->proto_paths[i]);
    }
#else
    if (schema->proto_paths_len > 0) {
        setError(ctx, "Proto paths require .proto source parsing support.");
        return false;
    }
#endif

    std::unique_ptr<google::protobuf::DescriptorPool> pool;
    if (schema->schema_type == OPENZL_PROTOBUF_SCHEMA_PROTO) {
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
        pool = loader.loadProtoFile(schema->schema_path);
#else
        setError(ctx, ".proto source parsing support is disabled.");
        return false;
#endif
    } else if (schema->schema_type == OPENZL_PROTOBUF_SCHEMA_DESCRIPTOR) {
        pool = loader.loadDescriptorFile(schema->schema_path);
    } else {
        setError(ctx, "Unknown schema type.");
        return false;
    }

    if (!pool) {
        setError(ctx, "Failed to load protobuf schema.");
        return false;
    }

    ctx->descriptor_pool = std::move(pool);
    ctx->message_helper =
            std::make_unique<DynamicMessageHelper>(ctx->descriptor_pool.get());
    ctx->message_type = schema->message_type;
    return true;
}
} // namespace

OpenZLProtobufContext* openzl_protobuf_create(
        const OpenZLProtobufSchema* schema)
{
    auto* ctx = new (std::nothrow) OpenZLProtobufContext();
    if (!ctx) {
        return nullptr;
    }

    try {
        clearError(ctx);
        ctx->ready = initializeContext(ctx, schema);
    } catch (const std::exception& ex) {
        setError(ctx, ex.what());
        ctx->ready = false;
    } catch (...) {
        setError(ctx, "Unknown error during initialization.");
        ctx->ready = false;
    }

    return ctx;
}

void openzl_protobuf_destroy(OpenZLProtobufContext* ctx)
{
    delete ctx;
}

const char* openzl_protobuf_last_error(const OpenZLProtobufContext* ctx)
{
    if (!ctx) {
        return kNullContextError;
    }
    return ctx->last_error.c_str();
}

int openzl_protobuf_set_compressor(
        OpenZLProtobufContext* ctx,
        const uint8_t* compressor_data,
        size_t compressor_len)
{
    if (!ensureReady(ctx)) {
        return 0;
    }
    clearError(ctx);
    if (!compressor_data || compressor_len == 0) {
        setError(ctx, "Compressor data is empty.");
        return 0;
    }
    try {
        openzl::Compressor compressor;
        compressor.deserialize(
                std::string(reinterpret_cast<const char*>(compressor_data),
                            compressor_len));
        ctx->serializer.setCompressor(std::move(compressor));
        return 1;
    } catch (const std::exception& ex) {
        setError(ctx, ex.what());
        return 0;
    } catch (...) {
        setError(ctx, "Unknown error while setting compressor.");
        return 0;
    }
}

int openzl_protobuf_compress(
        OpenZLProtobufContext* ctx,
        const uint8_t* proto_data,
        size_t proto_len,
        OpenZLBuffer* out)
{
    if (!ensureReady(ctx)) {
        return 0;
    }
    clearError(ctx);
    if (!proto_data || proto_len == 0) {
        setError(ctx, "Input protobuf data is empty.");
        return 0;
    }

    try {
        auto message = ctx->message_helper->parseMessage(
                ctx->message_type, proto_data, proto_len);
        if (!message) {
            setError(ctx, "Failed to parse protobuf message.");
            return 0;
        }
        auto compressed = ctx->serializer.serialize(*message);
        return copyToBuffer(ctx, compressed, out) ? 1 : 0;
    } catch (const std::exception& ex) {
        setError(ctx, ex.what());
        return 0;
    } catch (...) {
        setError(ctx, "Unknown error during compression.");
        return 0;
    }
}

int openzl_protobuf_decompress(
        OpenZLProtobufContext* ctx,
        const uint8_t* zl_data,
        size_t zl_len,
        OpenZLBuffer* out)
{
    if (!ensureReady(ctx)) {
        return 0;
    }
    clearError(ctx);
    if (!zl_data || zl_len == 0) {
        setError(ctx, "Input OpenZL data is empty.");
        return 0;
    }

    try {
        auto message = ctx->message_helper->newMessage(ctx->message_type);
        if (!message) {
            setError(ctx, "Failed to create protobuf message.");
            return 0;
        }
        ctx->deserializer.deserialize(
                std::string(reinterpret_cast<const char*>(zl_data), zl_len),
                *message);
        std::string serialized;
        if (!message->SerializeToString(&serialized)) {
            setError(ctx, "Failed to serialize protobuf message.");
            return 0;
        }
        return copyToBuffer(ctx, serialized, out) ? 1 : 0;
    } catch (const std::exception& ex) {
        setError(ctx, ex.what());
        return 0;
    } catch (...) {
        setError(ctx, "Unknown error during decompression.");
        return 0;
    }
}

void openzl_protobuf_free_buffer(OpenZLBuffer* buffer)
{
    if (!buffer) {
        return;
    }
    delete[] buffer->data;
    buffer->data = nullptr;
    buffer->len  = 0;
}
