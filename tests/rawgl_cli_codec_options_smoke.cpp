// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "cli_graph.h"

#include <iostream>
#include <utility>

static constexpr unsigned int RAWGL_TEST_GL_SAMPLER_2D = 0x8B5E;

static rawgl::ShaderInterface
inspect_test_shader_interface(const void*, rawgl::ShaderProgramKind, const std::vector<std::string>&)
{
    rawgl::ShaderInterface shaderInterface;
    shaderInterface.success = true;

    rawgl::ShaderResourceInfo sampler;
    sampler.name = "InSample";
    sampler.resourceClass = rawgl::ShaderResourceClass::sampler;
    sampler.textureShape = rawgl::ShaderTextureShape::tex_2d;
    sampler.glType = RAWGL_TEST_GL_SAMPLER_2D;
    shaderInterface.samplers.push_back(std::move(sampler));

    return shaderInterface;
}

int
main()
{
    const char* argv[] = {
        "RawGL",
        "--pass_vertfrag",
        "shader.glsl",
        "--pass_size",
        "16",
        "16",
        "--in",
        "InSample",
        "input.png",
        "--in_backend",
        "native_only",
        "--in_jpeg_color_transform",
        "rgb",
        "--in_png_expand_transparency",
        "false",
        "--in_tiff_directory_index",
        "2",
        "--in_exr_channels",
        "rgba",
        "--in_jpeg2000_reduce_factor",
        "1",
        "--in_jpeg2000_layer_limit",
        "2",
        "--out",
        "OutSample",
        "output.exr",
        "--out_format",
        "rgba32f",
        "--out_channels",
        "4",
        "--out_alpha_channel",
        "3",
        "--out_bits",
        "16",
        "--out_jpeg_quality",
        "92",
        "--out_jpeg_progressive",
        "true",
        "--out_jpeg_optimize",
        "true",
        "--out_jpeg_subsampling",
        "444",
        "--out_png_compression",
        "2",
        "--out_png_interlace",
        "false",
        "--out_tiff_compression",
        "deflate",
        "--out_tiff_predictor",
        "float",
        "--out_tiff_layout",
        "tiled",
        "--out_tiff_tile_size",
        "128",
        "64",
        "--out_tiff_rows_per_strip",
        "32",
        "--out_tiff_big_tiff",
        "true",
        "--out_tiff_unassociated_alpha",
        "true",
        "--out_tiff_jpeg_quality",
        "81",
        "--out_tiff_deflate_level",
        "6",
        "--out_tiff_zstd_level",
        "9",
        "--out_tiff_lzma_preset",
        "3",
        "--out_tiff_webp_level",
        "72",
        "--out_tiff_webp_lossless",
        "false",
        "--out_tiff_webp_lossless_exact",
        "true",
        "--out_exr_compression",
        "zip",
        "--out_exr_layout",
        "tiled",
        "--out_exr_tile_size",
        "64",
        "32",
        "--out_exr_line_order",
        "decreasing_y",
        "--out_exr_dwa_level",
        "45.5",
        "--out_jpeg2000_lossless",
        "false",
        "--out_jpeg2000_compression_ratio",
        "8.5",
        "--out_jpeg2000_quality",
        "42.0",
    };

    rawgl::CommandLineRequest request;
    request.argc = static_cast<int>(sizeof(argv) / sizeof(argv[0]));
    request.argv = argv;

    rawgl::ShaderInterfaceInspector inspector;
    inspector.inspect = inspect_test_shader_interface;

    const rawgl::CliWorkflow workflow = rawgl::BuildCliWorkflowFromCommandLine(request, inspector);
    if (workflow.fileInputs.size() != 1u || workflow.fileOutputs.size() != 1u) {
        std::cerr << "Unexpected translated file binding counts." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecLoadOptions& load = workflow.fileInputs[0].codecOptions;
    if (!load.hasBackendPolicy || load.backendPolicy != rawgl::io::ImageLoadBackendPolicy::NativeOnly
        || !load.hasJpeg || load.jpeg.colorTransform != rawgl::io::JpegLoadColorTransform::Rgb
        || !load.hasPng || load.png.expandTransparency || !load.hasTiff || load.tiff.directoryIndex != 2u
        || !load.hasOpenExr || load.openExr.channelSelection != rawgl::io::OpenExrChannelSelection::Rgba
        || !load.hasJpeg2000 || load.jpeg2000.reduceFactor != 1u || load.jpeg2000.layerLimit != 2u) {
        std::cerr << "Input codec options were not translated correctly." << std::endl;
        return 1;
    }

    const rawgl::io::FileOutputBinding& output = workflow.fileOutputs[0];
    if (output.format != "rgba32f" || output.channels != 4 || output.alphaChannel != 3 || output.bits != 16) {
        std::cerr << "Output format options were not translated correctly." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecSaveOptions& save = output.codecOptions;
    if (!save.hasJpeg || save.jpeg.quality != 92 || !save.jpeg.progressive || !save.jpeg.optimize
        || save.jpeg.subsampling != rawgl::io::JpegChromaSubsampling::S444 || !save.hasPng
        || save.png.compressionLevel != 2 || save.png.interlaced || !save.hasTiff
        || save.tiff.compression != rawgl::io::TiffCompressionMode::Deflate
        || save.tiff.predictor != rawgl::io::TiffPredictorMode::Float
        || save.tiff.layout != rawgl::io::TiffStorageLayout::Tiled || save.tiff.tileWidth != 128u
        || save.tiff.tileHeight != 64u || save.tiff.rowsPerStrip != 32u || !save.tiff.forceBigTiff
        || !save.tiff.unassociatedAlpha || save.tiff.jpegQuality != 81u || save.tiff.deflateLevel != 6u
        || save.tiff.zstdLevel != 9u || save.tiff.lzmaPreset != 3u || save.tiff.webpLevel != 72u
        || save.tiff.webpLossless || !save.tiff.webpLosslessExact || !save.hasOpenExr
        || save.openExr.compression != rawgl::io::OpenExrCompressionMode::Zip
        || save.openExr.layout != rawgl::io::OpenExrStorageLayout::Tiled || save.openExr.tileWidth != 64u
        || save.openExr.tileHeight != 32u || save.openExr.lineOrder != rawgl::io::OpenExrLineOrder::DecreasingY
        || save.openExr.dwaCompressionLevel != 45.5f || !save.hasJpeg2000 || save.jpeg2000.lossless
        || save.jpeg2000.compressionRatio != 8.5f || save.jpeg2000.quality != 42.0f) {
        std::cerr << "Output codec options were not translated correctly." << std::endl;
        return 1;
    }

    return 0;
}
