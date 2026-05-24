// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#include <filesystem>

#include "openzl/zl_version.h"

#if defined(ZL_IS_FBCODE) && (ZL_IS_FBCODE == 1)
#    include "tools/cxx/Resources.h"
#endif

namespace openzl {
namespace protobuf {

std::filesystem::path getTestDataPath(const std::string& filename);

} // namespace protobuf
} // namespace openzl
