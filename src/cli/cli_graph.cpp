// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "cli_graph.h"

#include "cli_parser.h"
#include "common.h"
#include "graph_shared.h"
#include "sequence.h"
#include "path_utils.h"

#include <cctype>
#include <stdexcept>

namespace rawgl {
namespace {

struct GraphTranslationState {
    Pass* currentPass = nullptr;
    InputBinding* currentInput = nullptr;
    io::FileInputBinding* currentFileInput = nullptr;
    io::FileOutputBinding* currentOutput = nullptr;
    ShaderInterface currentShaderInterface;
    std::string previousFormat = "rgb32f";
    int previousChannels       = 3;
    int previousAlphaChannel   = -1;
    int previousBits           = 16;
    size_t currentPassIndex    = 0;
    std::vector<io::FileInputBinding> fileInputs;
    std::vector<io::FileOutputBinding> fileOutputs;
};

static ShaderInterface
inspect_shader_interface(const ShaderInterfaceInspector& inspector,
                         const ShaderProgramKind kind,
                         const std::vector<std::string>& paths)
{
    if (!inspector.inspect) {
        throw std::runtime_error("Shader interface inspector is not configured.");
    }

    return inspector.inspect(inspector.userData, kind, paths);
}

static InputSourceKind
to_input_source_kind(const GraphInputSourceKind sourceKind)
{
    switch (sourceKind) {
    case GraphInputSourceKind::intValues: return InputSourceKind::intValues;
    case GraphInputSourceKind::uintValues: return InputSourceKind::uintValues;
    case GraphInputSourceKind::floatValues: return InputSourceKind::floatValues;
    case GraphInputSourceKind::doubleValues: return InputSourceKind::doubleValues;
    case GraphInputSourceKind::hostTexture: return InputSourceKind::hostTexture;
    case GraphInputSourceKind::passOutput: return InputSourceKind::passOutput;
    case GraphInputSourceKind::graphTexture: return InputSourceKind::workflowTexture;
    }

    return InputSourceKind::intValues;
}

static std::vector<ShaderModuleDefinition>
build_shader_modules(const ShaderProgramKind programKind, const std::vector<std::string>& paths)
{
    std::vector<ShaderModuleDefinition> modules;
    modules.reserve(paths.size());

    if (programKind == ShaderProgramKind::compute) {
        if (paths.size() != 1u) {
            throw std::runtime_error("pass_comp: must have one shader file.");
        }

        ShaderModuleDefinition module;
        module.role = ShaderModuleRole::compute;
        module.sourceKind = ShaderModuleSourceKind::filePath;
        module.path = paths[0];
        modules.push_back(std::move(module));
        return modules;
    }

    if (paths.empty() || paths.size() > 2u) {
        throw std::runtime_error("pass_vertfrag: must have one combined shader file or two stage files.");
    }

    if (paths.size() == 1u) {
        ShaderModuleDefinition module;
        module.role = ShaderModuleRole::automatic;
        module.sourceKind = ShaderModuleSourceKind::filePath;
        module.path = paths[0];
        modules.push_back(std::move(module));
        return modules;
    }

    ShaderModuleDefinition vertexModule;
    vertexModule.role = ShaderModuleRole::vertex;
    vertexModule.sourceKind = ShaderModuleSourceKind::filePath;
    vertexModule.path = paths[0];
    modules.push_back(std::move(vertexModule));

    ShaderModuleDefinition fragmentModule;
    fragmentModule.role = ShaderModuleRole::fragment;
    fragmentModule.sourceKind = ShaderModuleSourceKind::filePath;
    fragmentModule.path = paths[1];
    modules.push_back(std::move(fragmentModule));
    return modules;
}

static std::vector<std::string>
build_module_paths(const std::vector<ShaderModuleDefinition>& modules)
{
    std::vector<std::string> paths;
    paths.reserve(modules.size());
    for (const ShaderModuleDefinition& module : modules) {
        paths.push_back(module.path);
    }
    return paths;
}

template<typename T>
static T
parse_numeric_value(const std::string& text, const char* context)
{
    hres hr = hres::OK;
    const T value = str_to_numeric<T>(hr, text);
    if (hr != hres::OK) {
        throw std::runtime_error(std::string(context) + ": invalid numeric value: " + text);
    }
    return value;
}

static int
parse_positive_int(const std::string& text, const char* context)
{
    const int value = parse_numeric_value<int32_t>(text, context);
    if (value <= 0) {
        throw std::runtime_error(std::string(context) + ": value must be > 0");
    }
    return value;
}

static uint32_t
parse_non_negative_u32(const std::string& text, const char* context)
{
    const int32_t value = parse_numeric_value<int32_t>(text, context);
    if (value < 0) {
        throw std::runtime_error(std::string(context) + ": value must be a non-negative integer");
    }
    return static_cast<uint32_t>(value);
}

static uint32_t
parse_positive_u32(const std::string& text, const char* context)
{
    const uint32_t value = parse_non_negative_u32(text, context);
    if (value == 0u) {
        throw std::runtime_error(std::string(context) + ": value must be > 0");
    }
    return value;
}

static int
parse_bounded_int(const std::string& text, const char* context, const int minValue, const int maxValue)
{
    const int value = parse_numeric_value<int32_t>(text, context);
    if (value < minValue || value > maxValue) {
        throw std::runtime_error(std::string(context) + ": value is outside the supported range");
    }
    return value;
}

static std::string
normalize_option_value(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (const char ch : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

static bool
parse_bool_value(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        return false;
    }
    throw std::runtime_error(std::string(context) + ": expected true or false");
}

static io::ImageLoadBackendPolicy
parse_image_load_backend_policy(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "auto") {
        return io::ImageLoadBackendPolicy::Auto;
    }
    if (value == "native" || value == "native_only") {
        return io::ImageLoadBackendPolicy::NativeOnly;
    }
    if (value == "openimageio" || value == "openimageio_only" || value == "oiio" || value == "oiio_only") {
        return io::ImageLoadBackendPolicy::OpenImageIoOnly;
    }
    throw std::runtime_error(std::string(context) + ": unsupported load backend: " + text);
}

static io::JpegLoadColorTransform
parse_jpeg_load_color_transform(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "auto") {
        return io::JpegLoadColorTransform::Auto;
    }
    if (value == "rgb") {
        return io::JpegLoadColorTransform::Rgb;
    }
    if (value == "grayscale" || value == "gray") {
        return io::JpegLoadColorTransform::Grayscale;
    }
    throw std::runtime_error(std::string(context) + ": unsupported JPEG color transform: " + text);
}

static io::OpenExrChannelSelection
parse_openexr_channel_selection(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "auto") {
        return io::OpenExrChannelSelection::Auto;
    }
    if (value == "luminance" || value == "luma") {
        return io::OpenExrChannelSelection::Luminance;
    }
    if (value == "rgb") {
        return io::OpenExrChannelSelection::Rgb;
    }
    if (value == "rgba") {
        return io::OpenExrChannelSelection::Rgba;
    }
    if (value == "all") {
        return io::OpenExrChannelSelection::All;
    }
    throw std::runtime_error(std::string(context) + ": unsupported OpenEXR channel selection: " + text);
}

static io::JpegChromaSubsampling
parse_jpeg_chroma_subsampling(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "default") {
        return io::JpegChromaSubsampling::Default;
    }
    if (value == "444" || value == "4:4:4") {
        return io::JpegChromaSubsampling::S444;
    }
    if (value == "422" || value == "4:2:2") {
        return io::JpegChromaSubsampling::S422;
    }
    if (value == "420" || value == "4:2:0") {
        return io::JpegChromaSubsampling::S420;
    }
    if (value == "440" || value == "4:4:0") {
        return io::JpegChromaSubsampling::S440;
    }
    if (value == "411" || value == "4:1:1") {
        return io::JpegChromaSubsampling::S411;
    }
    throw std::runtime_error(std::string(context) + ": unsupported JPEG subsampling: " + text);
}

static io::TiffCompressionMode
parse_tiff_compression_mode(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "none") {
        return io::TiffCompressionMode::None;
    }
    if (value == "lzw") {
        return io::TiffCompressionMode::Lzw;
    }
    if (value == "packbits") {
        return io::TiffCompressionMode::PackBits;
    }
    if (value == "zip" || value == "deflate") {
        return io::TiffCompressionMode::Deflate;
    }
    if (value == "adobe_deflate") {
        return io::TiffCompressionMode::AdobeDeflate;
    }
    if (value == "jpeg" || value == "jpg") {
        return io::TiffCompressionMode::Jpeg;
    }
    if (value == "lzma") {
        return io::TiffCompressionMode::Lzma;
    }
    if (value == "zstd") {
        return io::TiffCompressionMode::Zstd;
    }
    if (value == "webp") {
        return io::TiffCompressionMode::Webp;
    }
    if (value == "jxl") {
        return io::TiffCompressionMode::Jxl;
    }
    if (value == "jxl_dng") {
        return io::TiffCompressionMode::JxlDng;
    }
    if (value == "lerc") {
        return io::TiffCompressionMode::Lerc;
    }
    throw std::runtime_error(std::string(context) + ": unsupported TIFF compression: " + text);
}

static io::TiffPredictorMode
parse_tiff_predictor_mode(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "none" || value == "1") {
        return io::TiffPredictorMode::None;
    }
    if (value == "horizontal" || value == "2") {
        return io::TiffPredictorMode::Horizontal;
    }
    if (value == "float" || value == "3") {
        return io::TiffPredictorMode::Float;
    }
    throw std::runtime_error(std::string(context) + ": unsupported TIFF predictor: " + text);
}

static io::TiffStorageLayout
parse_tiff_storage_layout(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "strips" || value == "strip") {
        return io::TiffStorageLayout::Strips;
    }
    if (value == "tiled" || value == "tile") {
        return io::TiffStorageLayout::Tiled;
    }
    throw std::runtime_error(std::string(context) + ": unsupported TIFF layout: " + text);
}

static io::OpenExrCompressionMode
parse_openexr_compression_mode(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "none") {
        return io::OpenExrCompressionMode::None;
    }
    if (value == "rle") {
        return io::OpenExrCompressionMode::Rle;
    }
    if (value == "zips") {
        return io::OpenExrCompressionMode::Zips;
    }
    if (value == "zip") {
        return io::OpenExrCompressionMode::Zip;
    }
    if (value == "piz") {
        return io::OpenExrCompressionMode::Piz;
    }
    if (value == "pxr24") {
        return io::OpenExrCompressionMode::Pxr24;
    }
    if (value == "b44") {
        return io::OpenExrCompressionMode::B44;
    }
    if (value == "b44a") {
        return io::OpenExrCompressionMode::B44A;
    }
    if (value == "dwaa") {
        return io::OpenExrCompressionMode::Dwaa;
    }
    if (value == "dwab") {
        return io::OpenExrCompressionMode::Dwab;
    }
    if (value == "htj2k256") {
        return io::OpenExrCompressionMode::Htj2k256;
    }
    if (value == "htj2k32") {
        return io::OpenExrCompressionMode::Htj2k32;
    }
    throw std::runtime_error(std::string(context) + ": unsupported OpenEXR compression: " + text);
}

static io::OpenExrStorageLayout
parse_openexr_storage_layout(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "scanlines" || value == "scanline") {
        return io::OpenExrStorageLayout::Scanlines;
    }
    if (value == "tiled" || value == "tile") {
        return io::OpenExrStorageLayout::Tiled;
    }
    throw std::runtime_error(std::string(context) + ": unsupported OpenEXR layout: " + text);
}

static io::OpenExrLineOrder
parse_openexr_line_order(const std::string& text, const char* context)
{
    const std::string value = normalize_option_value(text);
    if (value == "increasing_y" || value == "increasing") {
        return io::OpenExrLineOrder::IncreasingY;
    }
    if (value == "decreasing_y" || value == "decreasing") {
        return io::OpenExrLineOrder::DecreasingY;
    }
    if (value == "random_y" || value == "random") {
        return io::OpenExrLineOrder::RandomY;
    }
    throw std::runtime_error(std::string(context) + ": unsupported OpenEXR line order: " + text);
}

static const ShaderResourceInfo*
find_shader_resource(const ShaderInterface& shaderInterface, const std::string& name)
{
    auto find_in = [&name](const auto& resources) -> const ShaderResourceInfo* {
        for (const ShaderResourceInfo& resource : resources) {
            if (resource.name == name) {
                return &resource;
            }
        }
        return nullptr;
    };

    if (const ShaderResourceInfo* resource = find_in(shaderInterface.uniforms)) {
        return resource;
    }
    if (const ShaderResourceInfo* resource = find_in(shaderInterface.samplers)) {
        return resource;
    }
    if (const ShaderResourceInfo* resource = find_in(shaderInterface.images)) {
        return resource;
    }
    if (const ShaderResourceInfo* resource = find_in(shaderInterface.systemUniforms)) {
        return resource;
    }

    return nullptr;
}

static void
translate_pass_declaration(const CommandLineParsedOption& option,
                          Workflow& workflow,
                          const ShaderInterfaceInspector& inspectShaderInterface,
                          GraphTranslationState& state)
{
    Pass pass;
    pass.programKind = (option.string_key == "pass_comp") ? ShaderProgramKind::compute : ShaderProgramKind::vertfrag;
    pass.shaderModules = build_shader_modules(pass.programKind, option.value);
    if (!workflow.passes.empty()) {
        pass.sizeX = workflow.passes.back().sizeX;
        pass.sizeY = workflow.passes.back().sizeY;
    }

    state.currentShaderInterface =
        inspect_shader_interface(inspectShaderInterface, pass.programKind, build_module_paths(pass.shaderModules));
    if (!state.currentShaderInterface.success) {
        throw std::runtime_error(state.currentShaderInterface.errorMessage.empty()
                                     ? "Failed to load program for graph translation."
                                     : state.currentShaderInterface.errorMessage);
    }

    workflow.passes.push_back(std::move(pass));
    state.currentPass   = &workflow.passes.back();
    state.currentPassIndex = workflow.passes.size() - 1u;
    state.currentInput  = nullptr;
    state.currentFileInput = nullptr;
    state.currentOutput = nullptr;
}

static void
translate_pass_property(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass) {
        throw std::runtime_error(option.string_key + ": no preceding pass declaration.");
    }

    if (option.string_key == "pass_size") {
        if (option.value.empty() || option.value.size() > 2) {
            throw std::runtime_error("pass_size: must have 1 or 2 parameters.");
        }
        try {
            state.currentPass->sizeX = parse_positive_int(option.value[0], "pass_size");
            state.currentPass->sizeY =
                (option.value.size() > 1) ? parse_positive_int(option.value[1], "pass_size") : state.currentPass->sizeX;
        } catch (const std::exception&) {
            throw std::runtime_error("pass_size (" + option.value[0] + "): invalid numeric value");
        }
        return;
    }

    if (option.string_key == "pass_workgroupsize") {
        if (option.value.empty() || option.value.size() > 2) {
            throw std::runtime_error("pass_workgroupsize: must have 1 or 2 parameters.");
        }
        state.currentPass->workGroupSizeX = parse_positive_int(option.value[0], "pass_workgroupsize");
        state.currentPass->workGroupSizeY =
            (option.value.size() > 1) ? parse_positive_int(option.value[1], "pass_workgroupsize") : 1;
        state.currentPass->hasExplicitWorkGroupSize = true;
        return;
    }

    if (option.string_key == "bg_color") {
        if (option.value.empty() || option.value.size() > 4) {
            throw std::runtime_error("bg_color: must have 1 to 4 parameters.");
        }
        for (size_t i = 0; i < option.value.size(); ++i) {
            state.currentPass->clearColor[i] = parse_numeric_value<float_t>(option.value[i], "bg_color");
        }
        return;
    }

    if (option.string_key == "cull") {
        if (option.value.size() < 2 || (option.value.size() % 2) != 0) {
            throw std::runtime_error("cull: must have key/value pairs.");
        }
        for (size_t i = 0; i < option.value.size(); i += 2) {
            state.currentPass->cullParameters.push_back(Attribute { option.value[i], option.value[i + 1] });
        }
        return;
    }

    if (option.string_key != "pass_mesh") {
        return;
    }

    if (option.value.empty()) {
        throw std::runtime_error("pass_mesh: must have at least 1 parameter.");
    }

    MeshBinding mesh;
    const size_t split         = option.value[0].find("::");
    const std::string meshType = option.value[0].substr(0, split);

    if (meshType == "quad") {
        mesh.sourceKind = MeshSourceKind::quad;
        state.currentPass->meshes.push_back(std::move(mesh));
        return;
    }

    if (meshType != "mesh") {
        throw std::runtime_error("pass_mesh: unknown mesh type.");
    }
    if (split != std::string::npos) {
        throw std::runtime_error("pass_mesh: mesh references are not supported");
    }

    mesh.sourceKind = MeshSourceKind::file;
    for (size_t i = 1; i < option.value.size(); ++i) {
        const std::string extension = get_file_ext(option.value[i]);
        if (extension == "ply" || extension == "obj") {
            mesh.path = option.value[i];
            continue;
        }

        if (i + 1 >= option.value.size()) {
            throw std::runtime_error("pass_mesh: mesh attributes must be key/value pairs.");
        }

        mesh.parameters.push_back(Attribute { option.value[i], option.value[i + 1] });
        ++i;
    }

    if (mesh.path.empty()) {
        throw std::runtime_error("pass_mesh: mesh file path not found.");
    }

    state.currentPass->meshes.push_back(std::move(mesh));
}

static void
translate_input_option(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass || !state.currentShaderInterface.success) {
        throw std::runtime_error("in (" + option.value[0] + "): no preceding pass declaration.");
    }
    if (option.value.size() < 2) {
        throw std::runtime_error("in (" + option.value[0] + "): must have at least 2 parameters.");
    }

    const ShaderResourceInfo* shaderUniform = find_shader_resource(state.currentShaderInterface, option.value[0]);
    if (!shaderUniform) {
        throw std::runtime_error("in (" + option.value[0] + "): program uniform not found");
    }

    InputBinding input;
    input.name = option.value[0];
    io::FileInputBinding fileInput;
    fileInput.passIndex = state.currentPassIndex;
    fileInput.name = option.value[0];

    if (shaderUniform->glType == GL_SAMPLER_2D || shaderUniform->glType == GL_IMAGE_2D) {
        for (size_t i = 1; i < option.value.size(); ++i) {
            const std::string& token = option.value[i];

            if (token.find("::") != std::string::npos) {
                const size_t split = token.find("::");
                input.sourceKind                 = InputSourceKind::passOutput;
                input.referencedOutputName = token.substr(0, split);
                input.referencedPassIndex  = static_cast<size_t>(
                    parse_numeric_value<int32_t>(token.substr(split + 2), "in pass reference"));
                continue;
            }

            if (i + 1 < option.value.size()) {
                PassInput probeInput;
                hres attrResult = hres::OK;
                probeInput.eval_tex_attr(attrResult, token, option.value[i + 1]);
                if (attrResult == hres::OK) {
                    fileInput.attributes.push_back(Attribute { token, option.value[i + 1] });
                    ++i;
                    continue;
                }
            }

            fileInput.path = token;
        }

        if (input.sourceKind == InputSourceKind::passOutput) {
            state.currentPass->inputs.push_back(std::move(input));
            state.currentInput = &state.currentPass->inputs.back();
            state.currentFileInput = nullptr;
            return;
        }

        if (fileInput.path.empty()) {
            throw std::runtime_error("in (" + option.value[0] + "): texture path not found");
        }

        state.fileInputs.push_back(std::move(fileInput));
        state.currentInput = nullptr;
        state.currentFileInput = &state.fileInputs.back();
        return;
    } else {
        GraphInputSourceKind graphSourceKind = GraphInputSourceKind::intValues;
        uint8_t fieldCount = 0;
        if (!extract_numeric_layout(shaderUniform->glType, graphSourceKind, fieldCount)) {
            throw std::runtime_error("in (" + option.value[0] + "): unsupported uniform type");
        }
        input.sourceKind = to_input_source_kind(graphSourceKind);
        if ((option.value.size() - 1) < fieldCount) {
            throw std::runtime_error("in (" + option.value[0] + "): missing numeric values");
        }

        for (uint8_t i = 0; i < fieldCount; ++i) {
            const std::string& textValue = option.value[1 + i];
            switch (input.sourceKind) {
            case InputSourceKind::intValues:
                input.intValues.push_back(parse_numeric_value<int32_t>(textValue, "in"));
                break;
            case InputSourceKind::uintValues:
                input.uintValues.push_back(parse_numeric_value<uint32_t>(textValue, "in"));
                break;
            case InputSourceKind::floatValues:
                input.floatValues.push_back(parse_numeric_value<float_t>(textValue, "in"));
                break;
            case InputSourceKind::doubleValues:
                input.doubleValues.push_back(parse_numeric_value<double_t>(textValue, "in"));
                break;
            default: break;
            }
        }
    }

    state.currentPass->inputs.push_back(std::move(input));
    state.currentInput = &state.currentPass->inputs.back();
    state.currentFileInput = nullptr;
}

static void
translate_atomic_option(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass) {
        throw std::runtime_error("atomic: no preceding pass declaration.");
    }
    if (option.value.size() < 2) {
        throw std::runtime_error("atomic (" + option.value[0] + "): must have at least 2 parameters.");
    }
    if (option.value[0] != "cntr") {
        throw std::runtime_error("atomic (" + option.value[0] + "): unknown atomic buffer type");
    }
    if (option.value.size() > 3) {
        throw std::runtime_error("atomic (" + option.value[0] + "): can only have a single value");
    }

    CounterBinding counter;
    counter.name         = option.value[1];
    counter.initialValue = parse_numeric_value<uint32_t>(option.value[2], "atomic");
    state.currentPass->counters.push_back(std::move(counter));
}

static void
translate_input_attribute_option(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentInput) {
        if (!state.currentFileInput) {
            throw std::runtime_error("in_attr: no preceding input declaration.");
        }
        if (option.value.size() < 2) {
            throw std::runtime_error("in_attr: must have 2 parameters.");
        }
        state.currentFileInput->attributes.push_back(Attribute { option.value[0], option.value[1] });
        return;
    }
    if (option.value.size() < 2) {
        throw std::runtime_error("in_attr: must have 2 parameters.");
    }
    state.currentInput->attributes.push_back(Attribute { option.value[0], option.value[1] });
}

static bool
is_input_codec_option(const std::string& optionName)
{
    return optionName == "in_backend" || optionName == "in_jpeg_color_transform"
        || optionName == "in_png_expand_transparency" || optionName == "in_tiff_directory_index"
        || optionName == "in_exr_channels";
}

static bool
is_output_codec_option(const std::string& optionName)
{
    return optionName == "out_jpeg_quality" || optionName == "out_jpeg_progressive"
        || optionName == "out_jpeg_optimize" || optionName == "out_jpeg_subsampling"
        || optionName == "out_png_compression" || optionName == "out_png_interlace"
        || optionName == "out_tiff_compression" || optionName == "out_tiff_predictor"
        || optionName == "out_tiff_layout" || optionName == "out_tiff_tile_size"
        || optionName == "out_tiff_rows_per_strip" || optionName == "out_tiff_big_tiff"
        || optionName == "out_tiff_unassociated_alpha" || optionName == "out_tiff_jpeg_quality"
        || optionName == "out_tiff_deflate_level" || optionName == "out_tiff_zstd_level"
        || optionName == "out_tiff_lzma_preset" || optionName == "out_tiff_webp_level"
        || optionName == "out_tiff_webp_lossless" || optionName == "out_tiff_webp_lossless_exact"
        || optionName == "out_exr_compression" || optionName == "out_exr_layout"
        || optionName == "out_exr_tile_size" || optionName == "out_exr_line_order"
        || optionName == "out_exr_dwa_level";
}

static void
translate_input_codec_option(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentFileInput) {
        throw std::runtime_error(option.string_key + ": no preceding file input declaration.");
    }
    if (option.value.empty()) {
        throw std::runtime_error(option.string_key + ": missing value.");
    }

    io::ImageCodecLoadOptions& codecOptions = state.currentFileInput->codecOptions;
    if (option.string_key == "in_backend") {
        codecOptions.hasBackendPolicy = true;
        codecOptions.backendPolicy = parse_image_load_backend_policy(option.value[0], "in_backend");
        return;
    }

    if (option.string_key == "in_jpeg_color_transform") {
        codecOptions.hasJpeg = true;
        codecOptions.jpeg.hasColorTransform = true;
        codecOptions.jpeg.colorTransform =
            parse_jpeg_load_color_transform(option.value[0], "in_jpeg_color_transform");
        return;
    }

    if (option.string_key == "in_png_expand_transparency") {
        codecOptions.hasPng = true;
        codecOptions.png.hasExpandTransparency = true;
        codecOptions.png.expandTransparency = parse_bool_value(option.value[0], "in_png_expand_transparency");
        return;
    }

    if (option.string_key == "in_tiff_directory_index") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasDirectoryIndex = true;
        codecOptions.tiff.directoryIndex = parse_non_negative_u32(option.value[0], "in_tiff_directory_index");
        return;
    }

    if (option.string_key == "in_exr_channels") {
        codecOptions.hasOpenExr = true;
        codecOptions.openExr.hasChannelSelection = true;
        codecOptions.openExr.channelSelection =
            parse_openexr_channel_selection(option.value[0], "in_exr_channels");
        return;
    }
}

static void
apply_tiff_tile_size_option(const CommandLineParsedOption& option, io::TiffSaveOptions& options)
{
    if (option.value.empty() || option.value.size() > 2u) {
        throw std::runtime_error("out_tiff_tile_size: must have 1 or 2 parameters.");
    }

    options.hasTileWidth = true;
    options.tileWidth = parse_positive_u32(option.value[0], "out_tiff_tile_size");
    options.hasTileHeight = true;
    options.tileHeight = (option.value.size() == 2u)
        ? parse_positive_u32(option.value[1], "out_tiff_tile_size")
        : options.tileWidth;
}

static void
apply_openexr_tile_size_option(const CommandLineParsedOption& option, io::OpenExrSaveOptions& options)
{
    if (option.value.empty() || option.value.size() > 2u) {
        throw std::runtime_error("out_exr_tile_size: must have 1 or 2 parameters.");
    }

    options.hasTileWidth = true;
    options.tileWidth = parse_positive_u32(option.value[0], "out_exr_tile_size");
    options.hasTileHeight = true;
    options.tileHeight = (option.value.size() == 2u)
        ? parse_positive_u32(option.value[1], "out_exr_tile_size")
        : options.tileWidth;
}

static void
translate_output_codec_option(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentOutput) {
        throw std::runtime_error(option.string_key + ": no preceding output declaration.");
    }
    if (option.value.empty()) {
        throw std::runtime_error(option.string_key + ": missing value.");
    }

    io::ImageCodecSaveOptions& codecOptions = state.currentOutput->codecOptions;
    if (option.string_key == "out_jpeg_quality") {
        codecOptions.hasJpeg = true;
        codecOptions.jpeg.hasQuality = true;
        codecOptions.jpeg.quality = parse_bounded_int(option.value[0], "out_jpeg_quality", 1, 100);
        return;
    }

    if (option.string_key == "out_jpeg_progressive") {
        codecOptions.hasJpeg = true;
        codecOptions.jpeg.hasProgressive = true;
        codecOptions.jpeg.progressive = parse_bool_value(option.value[0], "out_jpeg_progressive");
        return;
    }

    if (option.string_key == "out_jpeg_optimize") {
        codecOptions.hasJpeg = true;
        codecOptions.jpeg.hasOptimize = true;
        codecOptions.jpeg.optimize = parse_bool_value(option.value[0], "out_jpeg_optimize");
        return;
    }

    if (option.string_key == "out_jpeg_subsampling") {
        codecOptions.hasJpeg = true;
        codecOptions.jpeg.hasSubsampling = true;
        codecOptions.jpeg.subsampling = parse_jpeg_chroma_subsampling(option.value[0], "out_jpeg_subsampling");
        return;
    }

    if (option.string_key == "out_png_compression") {
        codecOptions.hasPng = true;
        codecOptions.png.hasCompressionLevel = true;
        codecOptions.png.compressionLevel = parse_bounded_int(option.value[0], "out_png_compression", 0, 9);
        return;
    }

    if (option.string_key == "out_png_interlace") {
        codecOptions.hasPng = true;
        codecOptions.png.hasInterlaced = true;
        codecOptions.png.interlaced = parse_bool_value(option.value[0], "out_png_interlace");
        return;
    }

    if (option.string_key == "out_tiff_compression") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasCompression = true;
        codecOptions.tiff.compression = parse_tiff_compression_mode(option.value[0], "out_tiff_compression");
        return;
    }

    if (option.string_key == "out_tiff_predictor") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasPredictor = true;
        codecOptions.tiff.predictor = parse_tiff_predictor_mode(option.value[0], "out_tiff_predictor");
        return;
    }

    if (option.string_key == "out_tiff_layout") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasLayout = true;
        codecOptions.tiff.layout = parse_tiff_storage_layout(option.value[0], "out_tiff_layout");
        return;
    }

    if (option.string_key == "out_tiff_tile_size") {
        codecOptions.hasTiff = true;
        apply_tiff_tile_size_option(option, codecOptions.tiff);
        return;
    }

    if (option.string_key == "out_tiff_rows_per_strip") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasRowsPerStrip = true;
        codecOptions.tiff.rowsPerStrip = parse_positive_u32(option.value[0], "out_tiff_rows_per_strip");
        return;
    }

    if (option.string_key == "out_tiff_big_tiff") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasForceBigTiff = true;
        codecOptions.tiff.forceBigTiff = parse_bool_value(option.value[0], "out_tiff_big_tiff");
        return;
    }

    if (option.string_key == "out_tiff_unassociated_alpha") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasUnassociatedAlpha = true;
        codecOptions.tiff.unassociatedAlpha = parse_bool_value(option.value[0], "out_tiff_unassociated_alpha");
        return;
    }

    if (option.string_key == "out_tiff_jpeg_quality") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasJpegQuality = true;
        codecOptions.tiff.jpegQuality =
            static_cast<uint32_t>(parse_bounded_int(option.value[0], "out_tiff_jpeg_quality", 1, 100));
        return;
    }

    if (option.string_key == "out_tiff_deflate_level") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasDeflateLevel = true;
        codecOptions.tiff.deflateLevel =
            static_cast<uint32_t>(parse_bounded_int(option.value[0], "out_tiff_deflate_level", 1, 9));
        return;
    }

    if (option.string_key == "out_tiff_zstd_level") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasZstdLevel = true;
        codecOptions.tiff.zstdLevel =
            static_cast<uint32_t>(parse_bounded_int(option.value[0], "out_tiff_zstd_level", 1, 22));
        return;
    }

    if (option.string_key == "out_tiff_lzma_preset") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasLzmaPreset = true;
        codecOptions.tiff.lzmaPreset =
            static_cast<uint32_t>(parse_bounded_int(option.value[0], "out_tiff_lzma_preset", 0, 9));
        return;
    }

    if (option.string_key == "out_tiff_webp_level") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasWebpLevel = true;
        codecOptions.tiff.webpLevel =
            static_cast<uint32_t>(parse_bounded_int(option.value[0], "out_tiff_webp_level", 0, 100));
        return;
    }

    if (option.string_key == "out_tiff_webp_lossless") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasWebpLossless = true;
        codecOptions.tiff.webpLossless = parse_bool_value(option.value[0], "out_tiff_webp_lossless");
        return;
    }

    if (option.string_key == "out_tiff_webp_lossless_exact") {
        codecOptions.hasTiff = true;
        codecOptions.tiff.hasWebpLosslessExact = true;
        codecOptions.tiff.webpLosslessExact = parse_bool_value(option.value[0], "out_tiff_webp_lossless_exact");
        return;
    }

    if (option.string_key == "out_exr_compression") {
        codecOptions.hasOpenExr = true;
        codecOptions.openExr.hasCompression = true;
        codecOptions.openExr.compression = parse_openexr_compression_mode(option.value[0], "out_exr_compression");
        return;
    }

    if (option.string_key == "out_exr_layout") {
        codecOptions.hasOpenExr = true;
        codecOptions.openExr.hasLayout = true;
        codecOptions.openExr.layout = parse_openexr_storage_layout(option.value[0], "out_exr_layout");
        return;
    }

    if (option.string_key == "out_exr_tile_size") {
        codecOptions.hasOpenExr = true;
        apply_openexr_tile_size_option(option, codecOptions.openExr);
        return;
    }

    if (option.string_key == "out_exr_line_order") {
        codecOptions.hasOpenExr = true;
        codecOptions.openExr.hasLineOrder = true;
        codecOptions.openExr.lineOrder = parse_openexr_line_order(option.value[0], "out_exr_line_order");
        return;
    }

    if (option.string_key == "out_exr_dwa_level") {
        codecOptions.hasOpenExr = true;
        codecOptions.openExr.hasDwaCompressionLevel = true;
        codecOptions.openExr.dwaCompressionLevel =
            parse_numeric_value<float_t>(option.value[0], "out_exr_dwa_level");
        return;
    }
}

static void
translate_output_option(const CommandLineParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass || !state.currentShaderInterface.success) {
        throw std::runtime_error(option.string_key + ": no preceding pass declaration.");
    }

    if (option.string_key == "out") {
        if (option.value.size() != 2) {
            throw std::runtime_error("out: must have 2 parameters.");
        }

        io::FileOutputBinding output;
        output.passIndex    = state.currentPassIndex;
        output.name         = option.value[0];
        output.path         = option.value[1];
        output.format       = state.previousFormat;
        output.channels     = state.previousChannels;
        output.alphaChannel = state.previousAlphaChannel;
        output.bits         = state.previousBits;

        state.fileOutputs.push_back(std::move(output));
        state.currentOutput = &state.fileOutputs.back();
        return;
    }

    if (!state.currentOutput) {
        throw std::runtime_error(option.string_key + ": no preceding output declaration.");
    }

    if (option.string_key == "out_format") {
        state.currentOutput->format = option.value[0];
        state.previousFormat        = option.value[0];
        return;
    }

    if (option.string_key == "out_channels") {
        state.currentOutput->channels = parse_positive_int(option.value[0], "out_channels");
        state.previousChannels        = state.currentOutput->channels;
        return;
    }

    if (option.string_key == "out_alpha_channel") {
        state.currentOutput->alphaChannel = parse_numeric_value<int32_t>(option.value[0], "out_alpha_channel");
        state.previousAlphaChannel        = state.currentOutput->alphaChannel;
        return;
    }

    if (option.string_key == "out_bits") {
        state.currentOutput->bits = parse_positive_int(option.value[0], "out_bits");
        state.previousBits        = state.currentOutput->bits;
        return;
    }

    if (option.string_key == "out_attr") {
        if (option.value.size() < 2) {
            throw std::runtime_error("out_attr: must have 2 parameters.");
        }
        state.currentOutput->attributes.push_back(Attribute { option.value[0], option.value[1] });
    }
}

}  // namespace

CliWorkflow
BuildCliWorkflowFromCommandLine(const CommandLineRequest& request, const ShaderInterfaceInspector& inspectShaderInterface)
{
    const CommandLineParsedArguments parsedArguments = ParseCommandLineArguments(request.argc, request.argv);

    CliWorkflow result;
    result.workflow.verbosity = parsedArguments.verbosity;

    GraphTranslationState state;
    for (const CommandLineParsedOption& option : parsedArguments.options) {
        if (option.string_key == "pass_vertfrag" || option.string_key == "pass_comp") {
            translate_pass_declaration(option, result.workflow, inspectShaderInterface, state);
            continue;
        }

        if (option.string_key == "pass_size" || option.string_key == "pass_workgroupsize"
            || option.string_key == "bg_color" || option.string_key == "pass_mesh" || option.string_key == "cull") {
            translate_pass_property(option, state);
            continue;
        }

        if (option.string_key == "in") {
            translate_input_option(option, state);
            continue;
        }

        if (is_input_codec_option(option.string_key)) {
            translate_input_codec_option(option, state);
            continue;
        }

        if (option.string_key == "atomic") {
            translate_atomic_option(option, state);
            continue;
        }

        if (option.string_key == "in_attr") {
            translate_input_attribute_option(option, state);
            continue;
        }

        if (is_output_codec_option(option.string_key)) {
            translate_output_codec_option(option, state);
            continue;
        }

        translate_output_option(option, state);
    }

    result.fileInputs = std::move(state.fileInputs);
    result.fileOutputs = std::move(state.fileOutputs);
    return result;
}

Workflow
BuildWorkflowFromCommandLine(const CommandLineRequest& request, const ShaderInterfaceInspector& inspectShaderInterface)
{
    return BuildCliWorkflowFromCommandLine(request, inspectShaderInterface).workflow;
}

}  // namespace rawgl
