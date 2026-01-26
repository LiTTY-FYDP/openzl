#include "tools/protobuf/openzl_protobuf_ffi.h"

#include <google/protobuf/message.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "custom_parsers/dependency_registration.h"
#include "openzl/cpp/CCtx.hpp"
#include "openzl/cpp/DCtx.hpp"
#include "openzl/zl_compress.h"
#include "tools/protobuf/DescriptorLoader.h"
#include "tools/protobuf/DynamicMessageHelper.h"
#include "tools/protobuf/ProtoDeserializer.h"
#include "tools/protobuf/ProtoSerializer.h"
#include "tools/training/clustering/clustering_graph_trainer.h"
#include "tools/training/train_params.h"
#include "tools/training/train.h"

struct OpenZLProtobufContext {
    openzl::protobuf::ProtoSerializer serializer;
    openzl::protobuf::ProtoDeserializer deserializer;
    std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool;
    std::unique_ptr<openzl::protobuf::DynamicMessageHelper> message_helper;
    std::string message_type;
    std::string last_error;
    bool ready = false;
};

namespace {
using openzl::protobuf::DescriptorLoader;
using openzl::protobuf::DynamicMessageHelper;
using openzl::protobuf::ProtoDeserializer;
using openzl::protobuf::ProtoSerializer;

const char* kNullContextError = "OpenZL Protobuf context is null.";
constexpr size_t kBenchmarkIters = 10;
constexpr size_t kBytesToMb      = 1000 * 1000;

struct BenchmarkResultLocal {
    double compressionRatio;
    double compressionSpeed;
    double decompressionSpeed;
};

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
    for (size_t i = 0; i < schema->proto_paths_len; ++i) {
        if (!schema->proto_paths[i]) {
            setError(ctx, "Proto path entry is null.");
            return false;
        }
        loader.addProtoPath(schema->proto_paths[i]);
    }

    std::unique_ptr<google::protobuf::DescriptorPool> pool;
    if (schema->schema_type == OPENZL_PROTOBUF_SCHEMA_PROTO) {
        pool = loader.loadProtoFile(schema->schema_path);
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

bool fillTrainParams(
        OpenZLProtobufContext* ctx,
        const OpenZLProtobufTrainParams* params,
        openzl::training::TrainParams* outParams)
{
    if (!outParams) {
        setError(ctx, "Training params output is null.");
        return false;
    }
    *outParams = openzl::training::TrainParams{};
    outParams->noAceSuccessors = true;
    outParams->noClustering    = false;

    if (!params) {
        return true;
    }

    if (params->has_threads) {
        outParams->threads = params->threads;
    }
    if (params->has_clustering_trainer) {
        switch (params->clustering_trainer) {
            case OPENZL_PROTOBUF_CLUSTERING_TRAINER_GREEDY:
                outParams->clusteringTrainer =
                        openzl::training::ClusteringTrainer::Greedy;
                break;
            case OPENZL_PROTOBUF_CLUSTERING_TRAINER_BOTTOM_UP:
                outParams->clusteringTrainer =
                        openzl::training::ClusteringTrainer::BottomUp;
                break;
            case OPENZL_PROTOBUF_CLUSTERING_TRAINER_FULL_SPLIT:
                outParams->clusteringTrainer =
                        openzl::training::ClusteringTrainer::FullSplit;
                break;
            default:
                setError(ctx, "Unknown clustering trainer.");
                return false;
        }
    }
    if (params->has_max_time_secs) {
        outParams->maxTimeSecs = params->max_time_secs;
    }
    if (params->has_no_ace_successors) {
        outParams->noAceSuccessors = params->no_ace_successors != 0;
    }
    if (params->has_no_clustering) {
        outParams->noClustering = params->no_clustering != 0;
    }
    return true;
}

openzl::CCtx createCompressionContext(
        const openzl::Compressor& compressor)
{
    openzl::CCtx cctx;
    cctx.setParameter(openzl::CParam::FormatVersion, ZL_MAX_FORMAT_VERSION);
    cctx.setParameter(openzl::CParam::StickyParameters, 1);
    cctx.setParameter(openzl::CParam::PermissiveCompression, 1);
    cctx.refCompressor(compressor);
    return cctx;
}

BenchmarkResultLocal benchmarkCompressor(
        const std::vector<openzl::training::MultiInput>& inputs,
        const openzl::Compressor& compressor)
{
    BenchmarkResultLocal result{};
    if (inputs.empty()) {
        throw std::runtime_error("No samples provided for benchmarking.");
    }
    auto cctx = createCompressionContext(compressor);
    openzl::DCtx dctx;

    auto cdur                      = std::chrono::nanoseconds::zero();
    auto ddur                      = std::chrono::nanoseconds::zero();
    size_t total_compressed_size   = 0;
    size_t total_uncompressed_size = 0;

    for (const auto& inputsEntry : inputs) {
        const auto& inputVec = *inputsEntry;
        size_t uncompressed_size = 0;
        for (const auto& input : inputVec) {
            uncompressed_size += input.contentSize();
        }
        const auto compressed = cctx.compress(inputVec);
        total_compressed_size += compressed.size();
        total_uncompressed_size += uncompressed_size;

        const auto compression_start = std::chrono::steady_clock::now();
        for (size_t n = 0; n < kBenchmarkIters; ++n) {
            const auto curr_compressed_size = cctx.compress(inputVec).size();
            if (curr_compressed_size != compressed.size()) {
                throw std::runtime_error("Non-deterministic compression!");
            }
        }
        const auto compression_end = std::chrono::steady_clock::now();

        const auto decompression_start = std::chrono::steady_clock::now();
        for (size_t n = 0; n < kBenchmarkIters; ++n) {
            auto decompressed = dctx.decompress(compressed);
            for (size_t i = 0; i < decompressed.size(); ++i) {
                if (decompressed[i].contentSize() != inputVec[i].contentSize()) {
                    throw std::runtime_error("Round-trip failure!");
                }
            }
        }
        const auto decompression_end = std::chrono::steady_clock::now();

        cdur += compression_end - compression_start;
        ddur += decompression_end - decompression_start;
    }

    if (total_compressed_size == 0 || total_uncompressed_size == 0) {
        throw std::runtime_error("Benchmarking produced empty sizes.");
    }
    const auto ratio =
            static_cast<double>(total_uncompressed_size) / total_compressed_size;
    const auto cmicros = std::chrono::duration<double, std::micro>(cdur);
    const auto dmicros = std::chrono::duration<double, std::micro>(ddur);
    const auto cmibps =
            (total_uncompressed_size * kBenchmarkIters * 1000 * 1000.0)
            / (cmicros.count() * kBytesToMb);
    const auto dmibps =
            (total_uncompressed_size * kBenchmarkIters * 1000 * 1000.0)
            / (dmicros.count() * kBytesToMb);

    result.compressionRatio   = ratio;
    result.compressionSpeed   = cmibps;
    result.decompressionSpeed = dmibps;
    return result;
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

int openzl_protobuf_train(
        OpenZLProtobufContext* ctx,
        const OpenZLBuffer* samples,
        size_t samples_len,
        OpenZLBuffer* out_compressor)
{
    return openzl_protobuf_train_with_params(
            ctx, samples, samples_len, nullptr, out_compressor);
}

int openzl_protobuf_train_with_params(
        OpenZLProtobufContext* ctx,
        const OpenZLBuffer* samples,
        size_t samples_len,
        const OpenZLProtobufTrainParams* params,
        OpenZLBuffer* out_compressor)
{
    if (!ensureReady(ctx)) {
        return 0;
    }
    clearError(ctx);
    if (!samples || samples_len == 0) {
        setError(ctx, "Training samples are empty.");
        return 0;
    }

    try {
        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        messages.reserve(samples_len);
        for (size_t i = 0; i < samples_len; ++i) {
            const auto& sample = samples[i];
            if (!sample.data || sample.len == 0) {
                setError(ctx, "Training sample is empty.");
                return 0;
            }
            auto message = ctx->message_helper->parseMessage(
                    ctx->message_type, sample.data, sample.len);
            if (!message) {
                setError(ctx, "Failed to parse training sample.");
                return 0;
            }
            messages.push_back(std::move(message));
        }

        std::vector<openzl::training::MultiInput> inputs(messages.size());
        for (size_t i = 0; i < messages.size(); ++i) {
            inputs[i] = openzl::training::MultiInput(
                    ctx->serializer.getTrainingInputs(*messages[i]));
        }

        auto compressor = ctx->serializer.getCompressor();
        openzl::training::TrainParams trainParams;
        if (!fillTrainParams(ctx, params, &trainParams)) {
            return 0;
        }

        auto serialized =
                openzl::training::trainClusteringGraph(inputs, *compressor, trainParams);
        if (!serialized) {
            setError(ctx, "Training returned no compressors.");
            return 0;
        }

        std::string out(*serialized);
        return copyToBuffer(ctx, out, out_compressor) ? 1 : 0;
    } catch (const std::exception& ex) {
        setError(ctx, ex.what());
        return 0;
    } catch (...) {
        setError(ctx, "Unknown error during training.");
        return 0;
    }
}

int openzl_protobuf_train_pareto(
        OpenZLProtobufContext* ctx,
        const OpenZLBuffer* samples,
        size_t samples_len,
        const OpenZLProtobufTrainParams* params,
        OpenZLProtobufParetoResult** out_results,
        size_t* out_results_len)
{
    if (!ensureReady(ctx)) {
        return 0;
    }
    clearError(ctx);
    if (!samples || samples_len == 0) {
        setError(ctx, "Training samples are empty.");
        return 0;
    }
    if (!out_results || !out_results_len) {
        setError(ctx, "Output result pointers are null.");
        return 0;
    }

    *out_results = nullptr;
    *out_results_len = 0;

    try {
        std::vector<std::unique_ptr<google::protobuf::Message>> messages;
        messages.reserve(samples_len);
        for (size_t i = 0; i < samples_len; ++i) {
            const auto& sample = samples[i];
            if (!sample.data || sample.len == 0) {
                setError(ctx, "Training sample is empty.");
                return 0;
            }
            auto message = ctx->message_helper->parseMessage(
                    ctx->message_type, sample.data, sample.len);
            if (!message) {
                setError(ctx, "Failed to parse training sample.");
                return 0;
            }
            messages.push_back(std::move(message));
        }

        std::vector<openzl::training::MultiInput> inputs(messages.size());
        for (size_t i = 0; i < messages.size(); ++i) {
            inputs[i] = openzl::training::MultiInput(
                    ctx->serializer.getTrainingInputs(*messages[i]));
        }

        auto compressor = ctx->serializer.getCompressor();
        openzl::training::TrainParams trainParams;
        if (!fillTrainParams(ctx, params, &trainParams)) {
            return 0;
        }
        trainParams.paretoFrontier = true;
        trainParams.compressorGenFunc =
                openzl::custom_parsers::createCompressorFromSerialized;

        auto serializedCompressorResults =
                openzl::training::train(inputs, *compressor, trainParams);
        if (serializedCompressorResults.empty()) {
            setError(ctx, "Training returned no compressors.");
            return 0;
        }

        std::vector<OpenZLProtobufParetoResult> results;
        results.reserve(serializedCompressorResults.size());
        for (size_t i = 0; i < serializedCompressorResults.size(); ++i) {
            auto& serialized = serializedCompressorResults[i];
            auto resultCompressor =
                    openzl::custom_parsers::createCompressorFromSerialized(*serialized);
            auto metrics = benchmarkCompressor(inputs, *resultCompressor);
            results.push_back(OpenZLProtobufParetoResult{
                    .index              = i,
                    .compression_ratio  = metrics.compressionRatio,
                    .compression_speed  = metrics.compressionSpeed,
                    .decompression_speed = metrics.decompressionSpeed,
            });
        }

        std::sort(
                results.begin(),
                results.end(),
                [](const OpenZLProtobufParetoResult& a,
                   const OpenZLProtobufParetoResult& b) {
                    return a.compression_ratio > b.compression_ratio;
                });

        auto* buffer = new (std::nothrow)
                OpenZLProtobufParetoResult[results.size()];
        if (!buffer) {
            setError(ctx, "Failed to allocate pareto results.");
            return 0;
        }
        for (size_t i = 0; i < results.size(); ++i) {
            buffer[i] = results[i];
        }
        *out_results = buffer;
        *out_results_len = results.size();
        return 1;
    } catch (const std::exception& ex) {
        setError(ctx, ex.what());
        return 0;
    } catch (...) {
        setError(ctx, "Unknown error during pareto training.");
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

void openzl_protobuf_free_pareto_results(OpenZLProtobufParetoResult* results)
{
    delete[] results;
}
