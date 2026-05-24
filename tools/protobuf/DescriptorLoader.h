// Copyright (c) Meta Platforms, Inc. and affiliates.
#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
#    include <google/protobuf/compiler/importer.h>
#endif
#include <memory>
#include <string>
#include <vector>

namespace openzl {
namespace protobuf {

/**
 * Error collector for protobuf compiler that stores error messages
 */
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
class ErrorCollector
        : public google::protobuf::compiler::MultiFileErrorCollector {
   public:
#if GOOGLE_PROTOBUF_VERSION > 4026000
    void RecordError(
            absl::string_view filename,
            int line,
            int column,
            absl::string_view message) override
#else
    void AddError(
            const std::string& filename,
            int line,
            int column,
            const std::string& message) override
#endif
    {
        errors_.push_back(
                std::string(filename) + ":" + std::to_string(line) + ":"
                + std::to_string(column) + ": " + std::string(message));
    }

#if GOOGLE_PROTOBUF_VERSION > 4026000
    void RecordWarning(
            absl::string_view filename,
            int line,
            int column,
            absl::string_view message) override
#else
    void AddWarning(
            const std::string& filename,
            int line,
            int column,
            const std::string& message) override
#endif
    {
        warnings_.push_back(
                std::string(filename) + ":" + std::to_string(line) + ":"
                + std::to_string(column) + ": " + std::string(message));
    }

    const std::vector<std::string>& errors() const
    {
        return errors_;
    }
    const std::vector<std::string>& warnings() const
    {
        return warnings_;
    }

   private:
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
};
#endif

/**
 * Loads protobuf descriptors from compiled .desc files at runtime.
 *
 * Runtime .proto source parsing is available when the protobuf proto-source
 * build option is enabled.
 *
 * Example usage:
 *
 * // Load from .proto file
 * DescriptorLoader loader;
 * loader.addProtoPath("./protos");
 * loader.addProtoPath("/usr/include");
 * auto pool = loader.loadProtoFile("schema.proto");
 *
 * // Load from .desc file
 * DescriptorLoader loader;
 * auto pool = loader.loadDescriptorFile("schema.desc");
 */
class DescriptorLoader {
   public:
    DescriptorLoader();
    ~DescriptorLoader();

    /**
     * Add a search path for .proto file imports (like protoc --proto_path)
     */
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
    void addProtoPath(const std::string& path);

    /**
     * Load a .proto file and return a DescriptorPool with all types.
     * Throws std::runtime_error on parse errors.
     *
     * @param proto_file Path to the .proto file
     * @return DescriptorPool containing the loaded types
     */
    std::unique_ptr<google::protobuf::DescriptorPool> loadProtoFile(
            const std::string& proto_file);
#endif

    /**
     * Load a .desc file (FileDescriptorSet) and return a DescriptorPool.
     * Throws std::runtime_error on parse errors.
     *
     * @param desc_file Path to the .desc file
     * @return DescriptorPool containing the loaded types
     */
    std::unique_ptr<google::protobuf::DescriptorPool> loadDescriptorFile(
            const std::string& desc_file);

    /**
     * Get the list of errors from the last operation
     */
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
    const std::vector<std::string>& errors() const
    {
        return error_collector_->errors();
    }

    /**
     * Get the list of warnings from the last operation
     */
    const std::vector<std::string>& warnings() const
    {
        return error_collector_->warnings();
    }
#endif

   private:
#ifdef OPENZL_PROTOBUF_ENABLE_PROTO_SOURCE
    std::vector<std::string> proto_paths_;
    std::unique_ptr<ErrorCollector> error_collector_;
#endif
};

} // namespace protobuf
} // namespace openzl
