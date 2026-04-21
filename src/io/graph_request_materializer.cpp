// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_request_materializer.h"

#include "texture.h"

#include <shared_mutex>
#include <sstream>
#include <stdexcept>

namespace rawgl::io {
namespace {

static const IoRuntimeService&
resolve_io_runtime(const std::shared_ptr<IoRuntimeService>& ioRuntime)
{
    static const IoRuntimeService defaultIoRuntime;
    return ioRuntime ? *ioRuntime : defaultIoRuntime;
}

static std::string
build_texture_resource_key(const std::string& path, const std::vector<GraphAttribute>& attributes)
{
    std::ostringstream stream;
    stream << "file:" << path;
    for (const GraphAttribute& attribute : attributes) {
        stream << '\x1F' << attribute.name << '=' << attribute.value;
    }
    return stream.str();
}

static std::map<std::string, std::string>
to_attribute_map(const std::vector<GraphAttribute>& attributes)
{
    std::map<std::string, std::string> result;
    for (const GraphAttribute& attribute : attributes) {
        result.insert({ attribute.name, attribute.value });
    }
    return result;
}

static void
preload_cached_texture_resource(const RawGLContextState& contextState,
                                const std::string& path,
                                const std::vector<GraphAttribute>& attributes)
{
    const std::string cacheKey = build_texture_resource_key(path, attributes);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.textureCacheMutex);
        auto cacheIt = contextState.textureCache.find(cacheKey);
        if (cacheIt != contextState.textureCache.end()) {
            return;
        }
    }

    const HostImageData hostImage = resolve_io_runtime(contextState.ioRuntime).loadHostImageData(path,
                                                                                                  to_attribute_map(attributes));
    std::shared_ptr<Texture> texture =
        std::make_shared<Texture>(hostImage.width,
                                  hostImage.height,
                                  hostImage.glInternalFormat,
                                  hostImage.glType,
                                  hostImage.bytes.empty() ? nullptr : hostImage.bytes.data(),
                                  hostImage.alphaChannel);

    std::unique_lock<std::shared_mutex> writeLock(contextState.textureCacheMutex);
    contextState.textureCache.insert({ cacheKey, texture });
}

}  // namespace

void
materialize_graph_build_request(const RawGLContextState& contextState, const GraphBuildRequest& request)
{
    for (const GraphPassDefinition& pass : request.definition.passes) {
        for (const GraphInputDefinition& input : pass.inputs) {
            if (input.sourceKind != GraphInputSourceKind::textureFile) {
                continue;
            }

            preload_cached_texture_resource(contextState, input.texturePath, input.attributes);
        }
    }
}

GraphExecutionRequest
materialize_graph_execution_request(const std::shared_ptr<IoRuntimeService>& ioRuntime,
                                   const GraphExecutionRequest& request)
{
    GraphExecutionRequest materializedRequest = request;

    for (GraphInputOverride& inputOverride : materializedRequest.inputOverrides) {
        if (inputOverride.sourceKind != GraphInputSourceKind::textureFile) {
            continue;
        }

        const HostImageData hostImage =
            resolve_io_runtime(ioRuntime).loadHostImageData(inputOverride.texturePath,
                                                            to_attribute_map(inputOverride.attributes));
        inputOverride.sourceKind  = GraphInputSourceKind::hostTexture;
        inputOverride.hostTexture = std::make_shared<HostImageData>(hostImage);
        inputOverride.texturePath.clear();
    }

    return materializedRequest;
}

}  // namespace rawgl::io
