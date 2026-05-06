// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <GL/glew.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

static rawgl::io::ImageCodecLoadOptions
make_native_jpeg_load_options()
{
    rawgl::io::JpegLoadOptions jpeg;
    jpeg.hasColorTransform = true;
    jpeg.colorTransform = rawgl::io::JpegLoadColorTransform::Rgb;

    rawgl::io::ImageCodecLoadOptions codecOptions;
    codecOptions.hasBackendPolicy = true;
    codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
    codecOptions.hasJpeg = true;
    codecOptions.jpeg = jpeg;
    return codecOptions;
}

static rawgl::HostImageData
downsample_u8_image_nearest(const rawgl::HostImageData& source, const int maxEdge)
{
    rawgl::HostImageData image;
    image.channels = source.channels;
    image.alphaChannel = source.alphaChannel;
    image.glInternalFormat = source.glInternalFormat;
    image.glType = source.glType;

    const int longestEdge = std::max(source.width, source.height);
    if (longestEdge <= maxEdge) {
        image.width = source.width;
        image.height = source.height;
        image.bytes = source.bytes;
        return image;
    }

    const double scale = static_cast<double>(longestEdge) / static_cast<double>(maxEdge);
    image.width = std::max(1, static_cast<int>(std::lround(static_cast<double>(source.width) / scale)));
    image.height = std::max(1, static_cast<int>(std::lround(static_cast<double>(source.height) / scale)));
    image.bytes.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height)
                       * static_cast<size_t>(image.channels));

    const uint8_t* sourceBytes = reinterpret_cast<const uint8_t*>(source.bytes.data());
    uint8_t* destinationBytes = reinterpret_cast<uint8_t*>(image.bytes.data());
    for (int y = 0; y < image.height; ++y) {
        const int sourceY =
            std::min(source.height - 1, static_cast<int>((static_cast<double>(y) + 0.5) * scale));
        for (int x = 0; x < image.width; ++x) {
            const int sourceX =
                std::min(source.width - 1, static_cast<int>((static_cast<double>(x) + 0.5) * scale));
            const size_t sourcePixel =
                (static_cast<size_t>(sourceY) * static_cast<size_t>(source.width) + static_cast<size_t>(sourceX))
                * static_cast<size_t>(source.channels);
            const size_t destinationPixel =
                (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x))
                * static_cast<size_t>(image.channels);
            for (int channel = 0; channel < image.channels; ++channel) {
                destinationBytes[destinationPixel + static_cast<size_t>(channel)] =
                    sourceBytes[sourcePixel + static_cast<size_t>(channel)];
            }
        }
    }

    return image;
}

static rawgl::io::ImageCodecSaveOptions
make_png_save_options()
{
    rawgl::io::PngSaveOptions png;
    png.hasCompressionLevel = true;
    png.compressionLevel = 1;
    png.hasInterlaced = true;
    png.interlaced = false;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasPng = true;
    codecOptions.png = png;
    return codecOptions;
}

static rawgl::io::ImageCodecSaveOptions
make_tiff_save_options()
{
    rawgl::io::TiffSaveOptions tiff;
    tiff.hasCompression = true;
    tiff.compression = rawgl::io::TiffCompressionMode::Deflate;
    tiff.hasPredictor = true;
    tiff.predictor = rawgl::io::TiffPredictorMode::Horizontal;
    tiff.hasLayout = true;
    tiff.layout = rawgl::io::TiffStorageLayout::Tiled;
    tiff.hasTileWidth = true;
    tiff.tileWidth = 64;
    tiff.hasTileHeight = true;
    tiff.tileHeight = 64;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasTiff = true;
    codecOptions.tiff = tiff;
    return codecOptions;
}

static rawgl::io::ImageCodecSaveOptions
make_openexr_save_options()
{
    rawgl::io::OpenExrSaveOptions openExr;
    openExr.hasCompression = true;
    openExr.compression = rawgl::io::OpenExrCompressionMode::Zip;
    openExr.hasLayout = true;
    openExr.layout = rawgl::io::OpenExrStorageLayout::Tiled;
    openExr.hasTileWidth = true;
    openExr.tileWidth = 64;
    openExr.hasTileHeight = true;
    openExr.tileHeight = 64;
    openExr.hasLineOrder = true;
    openExr.lineOrder = rawgl::io::OpenExrLineOrder::IncreasingY;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasOpenExr = true;
    codecOptions.openExr = openExr;
    return codecOptions;
}

static rawgl::io::ImageCodecLoadOptions
make_native_png_load_options()
{
    rawgl::io::ImageCodecLoadOptions codecOptions;
    codecOptions.hasBackendPolicy = true;
    codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
    codecOptions.hasPng = true;
    return codecOptions;
}

static rawgl::io::ImageCodecLoadOptions
make_native_tiff_load_options()
{
    rawgl::io::TiffLoadOptions tiff;
    tiff.hasDirectoryIndex = true;
    tiff.directoryIndex = 0;

    rawgl::io::ImageCodecLoadOptions codecOptions;
    codecOptions.hasBackendPolicy = true;
    codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
    codecOptions.hasTiff = true;
    codecOptions.tiff = tiff;
    return codecOptions;
}

static rawgl::io::ImageCodecLoadOptions
make_native_openexr_load_options()
{
    rawgl::io::OpenExrLoadOptions openExr;
    openExr.hasChannelSelection = true;
    openExr.channelSelection = rawgl::io::OpenExrChannelSelection::Rgb;

    rawgl::io::ImageCodecLoadOptions codecOptions;
    codecOptions.hasBackendPolicy = true;
    codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
    codecOptions.hasOpenExr = true;
    codecOptions.openExr = openExr;
    return codecOptions;
}

static bool
save_image(const std::filesystem::path& path,
           const rawgl::HostImageData& image,
           const rawgl::io::ImageCodecSaveOptions& codecOptions,
           const int bits,
           const char* label)
{
    rawgl::io::ImageSaveRequest request;
    request.path = path.string();
    request.bits = bits;
    request.codecOptions = codecOptions;
    request.image = image;

    const rawgl::io::ImageSaveResult result = rawgl::io::SaveImageFile(request);
    if (!result.success) {
        std::cerr << label << " save failed: " << result.errorMessage << std::endl;
        return false;
    }

    return true;
}

static bool
verify_loaded_output(const std::filesystem::path& path,
                     const rawgl::io::ImageCodecLoadOptions& codecOptions,
                     const rawgl::HostImageData& source,
                     const uint32_t expectedGlType,
                     const char* label)
{
    rawgl::io::ImageLoadRequest request;
    request.path = path.string();
    request.codecOptions = codecOptions;

    const rawgl::io::ImageLoadResult result = rawgl::io::LoadImageFile(request);
    if (!result.success) {
        std::cerr << label << " reload failed: " << result.errorMessage << std::endl;
        return false;
    }

    if (result.image.width != source.width || result.image.height != source.height
        || result.image.channels != source.channels || result.image.glType != expectedGlType) {
        std::cerr << label << " reloaded shape or component type is unexpected." << std::endl;
        return false;
    }
    if (result.image.bytes.empty()) {
        std::cerr << label << " reloaded payload is empty." << std::endl;
        return false;
    }

    return true;
}

int
main()
{
    rawgl::io::ImageLoadRequest sourceRequest;
    sourceRequest.path = "tests/inputs/sky.jpg";
    sourceRequest.codecOptions = make_native_jpeg_load_options();

    const rawgl::io::ImageLoadResult sourceResult = rawgl::io::LoadImageFile(sourceRequest);
    if (!sourceResult.success) {
        std::cerr << "sky.jpg native load failed: " << sourceResult.errorMessage << std::endl;
        return 1;
    }
    if (sourceResult.image.channels != 3 || sourceResult.image.glType != GL_UNSIGNED_BYTE) {
        std::cerr << "sky.jpg native decode returned an unexpected host layout." << std::endl;
        return 1;
    }

    const rawgl::HostImageData source = downsample_u8_image_nearest(sourceResult.image, 640);

    const std::filesystem::path pngPath = "tests/outputs/rawgl_io_sky_codec_cpp.png";
    const std::filesystem::path tiffPath = "tests/outputs/rawgl_io_sky_codec_cpp.tif";
    const std::filesystem::path exrPath = "tests/outputs/rawgl_io_sky_codec_cpp.exr";

    std::error_code error;
    std::filesystem::create_directories(pngPath.parent_path(), error);
    std::filesystem::remove(pngPath, error);
    std::filesystem::remove(tiffPath, error);
    std::filesystem::remove(exrPath, error);

    if (!save_image(pngPath, source, make_png_save_options(), 16, "PNG")) {
        return 1;
    }
    if (!save_image(tiffPath, source, make_tiff_save_options(), 16, "TIFF")) {
        return 1;
    }
    if (!save_image(exrPath, source, make_openexr_save_options(), 16, "OpenEXR")) {
        return 1;
    }

    if (!verify_loaded_output(pngPath, make_native_png_load_options(), source, GL_UNSIGNED_SHORT, "PNG")) {
        return 1;
    }
    if (!verify_loaded_output(tiffPath, make_native_tiff_load_options(), source, GL_UNSIGNED_SHORT, "TIFF")) {
        return 1;
    }
    if (!verify_loaded_output(exrPath, make_native_openexr_load_options(), source, GL_HALF_FLOAT, "OpenEXR")) {
        return 1;
    }

    return 0;
}
