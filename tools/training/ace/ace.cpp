// Copyright (c) Meta Platforms, Inc. and affiliates.

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "openzl/cpp/Compressor.hpp"

#include "openzl/compress/cgraph.h"
#include "tools/logger/Logger.h"
#include "tools/training/ace/ace.h"
#include "tools/training/ace/ace_combination.h"
#include "tools/training/ace/ace_compressor.h"
#include "tools/training/ace/automated_compressor_explorer.h"
#include "tools/training/graph_mutation/graph_mutation_utils.h"
#include "tools/training/sample_collection/training_sample_collector.h"
#include "tools/training/utils/serialized_compressor_internal.h"
#include "tools/training/utils/utils.h"

namespace openzl::training {

const std::string ACE_GRAPH_NAME = "zl.ace";

namespace {
using namespace openzl::training::graph_mutation;
using namespace openzl::tools::logger;

openzl::Type preferredInputType(const std::vector<MultiInput>& samples)
{
    std::array<size_t, 4> counts{};
    std::array<size_t, 4> bytes{};
    bool hasFallback            = false;
    openzl::Type fallbackType   = openzl::Type::Serial;

    for (const auto& sample : samples) {
        for (const auto& input : *sample) {
            if (!hasFallback) {
                fallbackType = input.type();
                hasFallback  = true;
            }
            const auto idx = static_cast<size_t>(input.type());
            if (idx >= counts.size()) {
                continue;
            }
            if (input.contentSize() > 0) {
                counts[idx] += 1;
                bytes[idx] += input.contentSize();
            }
        }
    }

    if (!hasFallback) {
        return openzl::Type::Serial;
    }

    size_t bestIndex      = 0;
    size_t bestByteCount  = 0;
    size_t bestItemCount  = 0;
    for (size_t i = 0; i < counts.size(); ++i) {
        if (bytes[i] > bestByteCount
            || (bytes[i] == bestByteCount && counts[i] > bestItemCount)) {
            bestIndex     = i;
            bestByteCount = bytes[i];
            bestItemCount = counts[i];
        }
    }

    if (bestByteCount == 0 && bestItemCount == 0) {
        return fallbackType;
    }
    return static_cast<openzl::Type>(bestIndex);
}

std::vector<Input> flattenInputsByType(
        std::vector<MultiInput>& samples,
        openzl::Type type)
{
    std::vector<Input> flattened;
    for (auto& sample : samples) {
        for (auto& input : *sample) {
            if (input.type() == type) {
                flattened.push_back(InputRef(input.get()));
            }
        }
    }
    return flattened;
}

/**
 * @returns The Pareto-optimal set of compressors for @p samples.
 */
std::string trainBackend(
        std::vector<MultiInput>& samples,
        const TrainParams& trainParams,
        size_t graphIdx,
        size_t numGraphs)
{
    if (samples.empty()) {
        throw Exception(
                "There cannot be no samples for a backend graph to be trained.");
    }
    const auto type = preferredInputType(samples);
    auto flattened = flattenInputsByType(samples, type);
    if (flattened.empty()) {
        throw Exception(
                "No ACE inputs remain after filtering to a single type.");
    }
    poly::optional<std::chrono::seconds> maxTime;
    if (trainParams.maxTimeSecs.has_value()) {
        maxTime = std::chrono::seconds(trainParams.maxTimeSecs.value());
    } else {
        maxTime = poly::nullopt;
    }
    AutomatedCompressorExplorer::Parameters params{
        .numThreads = trainParams.threads.has_value()
                ? trainParams.threads.value()
                : std::thread::hardware_concurrency() / 2,
    };
    params.maxTime = maxTime;
    AutomatedCompressorExplorer ace(flattened, params);
    for (;;) {
        Logger::logProgress(
                INFO,
                ace.progress(),
                "Training ACE graph %u / %u: ACE progress",
                graphIdx,
                numGraphs);
        if (ace.finished()) {
            break;
        }
        ace.step();
    }
    Logger::finalizeProgress(INFO);
    return ace.savePopulation();
}

} // namespace

std::vector<SerializedCompressorInternal> ACETrainer::train(
        const std::vector<MultiInput>& inputs,
        std::string_view serializedCompressorInput,
        const TrainParams& trainParams)
{
    auto makeCompressor = [&serializedCompressorInput, &trainParams] {
        return std::move(
                *trainParams.compressorGenFunc(serializedCompressorInput));
    };
    auto compressor = makeCompressor();
    auto cctx       = refCCtxForTraining(compressor);

    const std::vector<GraphID> autoBackendGraphIDs =
            findAllGraphsWithPrefix(compressor, ACE_GRAPH_NAME);
    std::vector<std::string> autoBackendGraphs;
    autoBackendGraphs.reserve(autoBackendGraphIDs.size());
    for (const auto& graphID : autoBackendGraphIDs) {
        autoBackendGraphs.emplace_back(
                ZL_Compressor_Graph_getName(compressor.get(), graphID));
    }

    Logger::log(
            VERBOSE1,
            "Found ",
            autoBackendGraphs.size(),
            " ACE graphs in compressor");

    auto samples =
            collectInputStreamsForGraphs(inputs, autoBackendGraphs, cctx);

    std::unordered_map<
            std::string,
            std::vector<std::pair<ACECompressor, ACECompressionResult>>>
            candidates;

    std::unordered_map<std::string, LocalParams> paramsToReplace;

    size_t graphIdx        = 0;
    const size_t numGraphs = autoBackendGraphs.size();
    bool trainedGraph      = false;
    size_t skippedGraphs   = 0;
    for (const auto& backendGraph : autoBackendGraphs) {
        ++graphIdx;
        auto& graphSamples = samples[backendGraph];
        if (graphSamples.empty()) {
            Logger::log(
                    VERBOSE1,
                    "Skipping ACE graph ",
                    graphIdx,
                    " / ",
                    numGraphs,
                    " (",
                    backendGraph,
                    "): no training samples");
            skippedGraphs += 1;
            continue;
        }
        auto aceState = trainBackend(
                graphSamples, trainParams, graphIdx, numGraphs);
        auto localParams = LocalParams();
        localParams.addCopyParam(
                AutomatedCompressorExplorer::kAceStateParamId,
                aceState.data(),
                aceState.size());
        auto backendGraphID =
                ZL_Compressor_getGraph(compressor.get(), backendGraph.c_str());
        auto graphs = ZL_Compressor_Graph_getCustomGraphs(
                compressor.get(), backendGraphID);
        auto nodes = ZL_Compressor_Graph_getCustomNodes(
                compressor.get(), backendGraphID);
        auto gp = ZL_GraphParameters{ .customGraphs   = graphs.graphids,
                                      .nbCustomGraphs = graphs.nbGraphIDs,
                                      .customNodes    = nodes.nodeids,
                                      .nbCustomNodes  = nodes.nbNodeIDs,
                                      .localParams    = localParams.get() };
        compressor.unwrap(
                ZL_Compressor_overrideGraphParams(
                        compressor.get(), backendGraphID, &gp),
                "Graph replacement failed");
        trainedGraph = true;
    }
    if (skippedGraphs > 0) {
        Logger::log(
                VERBOSE1,
                "Skipped ",
                skippedGraphs,
                " ACE graphs with no samples.");
    }
    auto serializedCheckpoint = compressor.serialize();
    checkPoint_.emplace(std::string(serializedCheckpoint));
    if (!trainedGraph) {
        std::vector<SerializedCompressorInternal> result;
        result.emplace_back(std::move(serializedCheckpoint));
        return result;
    }
    return getCombinedCompressors(inputs, *checkPoint_, trainParams);
}
} // namespace openzl::training
