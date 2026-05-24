#pragma once

#include "tools/protobuf/openzl_protobuf_ffi.h"

#include <google/protobuf/descriptor.h>
#include <memory>
#include <string>

#include "tools/protobuf/DynamicMessageHelper.h"
#include "tools/protobuf/ProtoDeserializer.h"
#include "tools/protobuf/ProtoSerializer.h"

struct OpenZLProtobufContext {
    openzl::protobuf::ProtoSerializer serializer;
    openzl::protobuf::ProtoDeserializer deserializer;
    std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool;
    std::unique_ptr<openzl::protobuf::DynamicMessageHelper> message_helper;
    std::string message_type;
    std::string last_error;
    bool ready = false;
};

namespace openzl::protobuf::ffi {

void setError(OpenZLProtobufContext* ctx, std::string message);
void clearError(OpenZLProtobufContext* ctx);
bool ensureReady(OpenZLProtobufContext* ctx);

bool copyToBuffer(
        OpenZLProtobufContext* ctx,
        const std::string& data,
        OpenZLBuffer* out);

} // namespace openzl::protobuf::ffi
