// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if __has_include(<jpeglib.h>)
#    include <jpeglib.h>
#    define RAWGL_TEST_HAS_JPEGLIB 1
#endif

#if __has_include(<tiffio.h>)
#    include <tiffio.h>
#    define RAWGL_TEST_HAS_TIFFIO 1
#endif

#if __has_include(<OpenEXR/ImfTiledInputFile.h>)
#    include <OpenEXR/ImfTiledInputFile.h>
#    define RAWGL_TEST_HAS_OPENEXR_TILES 1
#endif

static void
write_u8_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const uint8_t value)
{
    bytes[sampleIndex] = static_cast<std::byte>(value);
}

static void
write_u16_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const uint16_t value)
{
    std::memcpy(bytes.data() + sampleIndex * sizeof(uint16_t), &value, sizeof(value));
}

static void
write_f32_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const float value)
{
    std::memcpy(bytes.data() + sampleIndex * sizeof(float), &value, sizeof(value));
}

static uint8_t
make_u8_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 13 + y * 31 + channel * 67);
    return static_cast<uint8_t>(value & 0xffu);
}

static uint16_t
make_u16_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 509 + y * 997 + channel * 8191);
    return static_cast<uint16_t>(value & 0xffffu);
}

static float
make_f32_sample_value(const int x, const int y, const int channel)
{
    return static_cast<float>(x) * 0.015625f + static_cast<float>(y) * 0.03125f
           + static_cast<float>(channel) * 0.125f;
}

static rawgl::HostImageData
make_u8_rgb_image(const int width, const int height)
{
    rawgl::HostImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB8;
    image.glType = GL_UNSIGNED_BYTE;
    image.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u
                    + static_cast<size_t>(channel);
                write_u8_sample(image.bytes, sampleIndex, make_u8_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static rawgl::HostImageData
make_u16_rgb_image(const int width, const int height)
{
    rawgl::HostImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB16;
    image.glType = GL_UNSIGNED_SHORT;
    image.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u * sizeof(uint16_t));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u
                    + static_cast<size_t>(channel);
                write_u16_sample(image.bytes, sampleIndex, make_u16_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static rawgl::HostImageData
make_f32_rgb_image(const int width, const int height)
{
    rawgl::HostImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB32F;
    image.glType = GL_FLOAT;
    image.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u * sizeof(float));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u
                    + static_cast<size_t>(channel);
                write_f32_sample(image.bytes, sampleIndex, make_f32_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static rawgl::io::ImageCodecLoadOptions
make_native_load_options()
{
    rawgl::io::ImageCodecLoadOptions options;
    options.hasBackendPolicy = true;
    options.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
    return options;
}

static bool
save_image_with_options(const std::filesystem::path& path,
                        const rawgl::HostImageData& image,
                        const int bits,
                        const std::vector<rawgl::Attribute>& attributes,
                        const rawgl::io::ImageCodecSaveOptions& codecOptions,
                        const char* label)
{
    rawgl::io::ImageSaveRequest request;
    request.path = path.string();
    request.bits = bits;
    request.attributes = attributes;
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
load_image_with_options(const std::filesystem::path& path,
                        const rawgl::io::ImageCodecLoadOptions& codecOptions,
                        rawgl::HostImageData& image,
                        const char* label)
{
    rawgl::io::ImageLoadRequest request;
    request.path = path.string();
    request.codecOptions = codecOptions;

    const rawgl::io::ImageLoadResult result = rawgl::io::LoadImageFile(request);
    if (!result.success) {
        std::cerr << label << " load failed: " << result.errorMessage << std::endl;
        return false;
    }

    image = result.image;
    return true;
}

static bool
verify_image_shape(const rawgl::HostImageData& image,
                   const int width,
                   const int height,
                   const int channels,
                   const uint32_t glType,
                   const char* label)
{
    if (image.width != width || image.height != height || image.channels != channels || image.glType != glType) {
        std::cerr << label << " loaded shape or GL type is unexpected." << std::endl;
        return false;
    }

    return true;
}

static bool
verify_lossless_round_trip(const rawgl::HostImageData& loaded,
                           const rawgl::HostImageData& source,
                           const char* label)
{
    if (!verify_image_shape(loaded, source.width, source.height, source.channels, source.glType, label)) {
        return false;
    }
    if (loaded.bytes != source.bytes) {
        std::cerr << label << " round-trip bytes differ from source." << std::endl;
        return false;
    }

    return true;
}

static bool
verify_jpeg_markers(const std::filesystem::path& path)
{
#if defined(RAWGL_TEST_HAS_JPEGLIB)
    FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr) {
        std::cerr << "Failed to reopen typed JPEG output." << std::endl;
        return false;
    }

    jpeg_decompress_struct cinfo {};
    jpeg_error_mgr errorManager {};
    cinfo.err = jpeg_std_error(&errorManager);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);

    const bool progressive = cinfo.progressive_mode != 0;
    const bool subsampling444 = cinfo.num_components == 3 && cinfo.comp_info[0].h_samp_factor == 1
                                && cinfo.comp_info[0].v_samp_factor == 1
                                && cinfo.comp_info[1].h_samp_factor == 1
                                && cinfo.comp_info[1].v_samp_factor == 1
                                && cinfo.comp_info[2].h_samp_factor == 1
                                && cinfo.comp_info[2].v_samp_factor == 1;

    jpeg_destroy_decompress(&cinfo);
    std::fclose(file);

    if (!progressive || !subsampling444) {
        std::cerr << "Typed JPEG save options were not reflected in the output file." << std::endl;
        return false;
    }
#else
    (void)path;
#endif

    return true;
}

static bool
verify_tiff_tiled_layout(const std::filesystem::path& path)
{
#if defined(RAWGL_TEST_HAS_TIFFIO)
    TIFF* tif = TIFFOpen(path.string().c_str(), "r");
    if (tif == nullptr) {
        std::cerr << "Failed to reopen typed TIFF output." << std::endl;
        return false;
    }

    const bool tiled = TIFFIsTiled(tif) != 0;
    uint32_t tileWidth = 0u;
    uint32_t tileLength = 0u;
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);
    TIFFClose(tif);

    if (!tiled || tileWidth != 16u || tileLength != 16u) {
        std::cerr << "Typed TIFF save options did not produce the requested tile layout." << std::endl;
        return false;
    }
#else
    (void)path;
#endif

    return true;
}

static bool
verify_openexr_tiled_layout(const std::filesystem::path& path)
{
#if defined(RAWGL_TEST_HAS_OPENEXR_TILES)
    try {
        OPENEXR_IMF_NAMESPACE::TiledInputFile input(path.string().c_str());
        if (input.tileXSize() != 8u || input.tileYSize() != 8u) {
            std::cerr << "Typed OpenEXR save options did not produce the requested tile layout." << std::endl;
            return false;
        }
    } catch (const std::exception& exception) {
        std::cerr << "Failed to reopen typed tiled OpenEXR: " << exception.what() << std::endl;
        return false;
    }
#else
    (void)path;
#endif

    return true;
}

static bool
verify_png_typed_options(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_u16_rgb_image(13, 11);

    rawgl::io::PngSaveOptions png;
    png.hasCompressionLevel = true;
    png.compressionLevel = 0;
    png.hasInterlaced = true;
    png.interlaced = false;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasPng = true;
    codecOptions.png = png;

    if (!save_image_with_options(path,
                                 source,
                                 16,
                                 { { "png:compression_level", "42" }, { "png:interlace", "maybe" } },
                                 codecOptions,
                                 "PNG typed matrix")) {
        return false;
    }

    rawgl::io::ImageCodecLoadOptions loadOptions = make_native_load_options();
    loadOptions.hasPng = true;
    loadOptions.png.hasExpandTransparency = true;
    loadOptions.png.expandTransparency = true;

    rawgl::HostImageData loaded;
    if (!load_image_with_options(path, loadOptions, loaded, "PNG typed matrix")) {
        return false;
    }

    return verify_lossless_round_trip(loaded, source, "PNG typed matrix");
}

static bool
verify_jpeg_typed_options(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_u8_rgb_image(19, 17);

    rawgl::io::JpegSaveOptions jpeg;
    jpeg.hasQuality = true;
    jpeg.quality = 96;
    jpeg.hasProgressive = true;
    jpeg.progressive = true;
    jpeg.hasOptimize = true;
    jpeg.optimize = true;
    jpeg.hasSubsampling = true;
    jpeg.subsampling = rawgl::io::JpegChromaSubsampling::S444;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasJpeg = true;
    codecOptions.jpeg = jpeg;

    if (!save_image_with_options(path,
                                 source,
                                 8,
                                 { { "jpeg:quality", "0" },
                                   { "jpeg:progressive", "maybe" },
                                   { "jpeg:optimize", "maybe" },
                                   { "jpeg:subsampling", "bad" } },
                                 codecOptions,
                                 "JPEG typed matrix")) {
        return false;
    }
    if (!verify_jpeg_markers(path)) {
        return false;
    }

    rawgl::io::ImageCodecLoadOptions rgbLoadOptions = make_native_load_options();
    rgbLoadOptions.hasJpeg = true;
    rgbLoadOptions.jpeg.hasColorTransform = true;
    rgbLoadOptions.jpeg.colorTransform = rawgl::io::JpegLoadColorTransform::Rgb;

    rawgl::HostImageData rgbLoaded;
    if (!load_image_with_options(path, rgbLoadOptions, rgbLoaded, "JPEG typed RGB matrix")) {
        return false;
    }
    if (!verify_image_shape(rgbLoaded, source.width, source.height, 3, GL_UNSIGNED_BYTE, "JPEG typed RGB matrix")) {
        return false;
    }
    if (rgbLoaded.bytes.empty()) {
        std::cerr << "JPEG typed RGB decode produced an empty payload." << std::endl;
        return false;
    }

    rawgl::io::ImageCodecLoadOptions grayLoadOptions = make_native_load_options();
    grayLoadOptions.hasJpeg = true;
    grayLoadOptions.jpeg.hasColorTransform = true;
    grayLoadOptions.jpeg.colorTransform = rawgl::io::JpegLoadColorTransform::Grayscale;

    rawgl::HostImageData grayLoaded;
    if (!load_image_with_options(path, grayLoadOptions, grayLoaded, "JPEG typed grayscale matrix")) {
        return false;
    }
    return verify_image_shape(grayLoaded, source.width, source.height, 1, GL_UNSIGNED_BYTE, "JPEG typed grayscale matrix");
}

static bool
verify_tiff_typed_options(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_u16_rgb_image(17, 19);

    rawgl::io::TiffSaveOptions tiff;
    tiff.hasCompression = true;
    tiff.compression = rawgl::io::TiffCompressionMode::None;
    tiff.hasLayout = true;
    tiff.layout = rawgl::io::TiffStorageLayout::Tiled;
    tiff.hasTileWidth = true;
    tiff.tileWidth = 16u;
    tiff.hasTileHeight = true;
    tiff.tileHeight = 16u;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasTiff = true;
    codecOptions.tiff = tiff;

    if (!save_image_with_options(path,
                                 source,
                                 16,
                                 { { "tiff:compression", "lzw" }, { "tiff:layout", "strips" } },
                                 codecOptions,
                                 "TIFF typed matrix")) {
        return false;
    }
    if (!verify_tiff_tiled_layout(path)) {
        return false;
    }

    rawgl::io::ImageCodecLoadOptions loadOptions = make_native_load_options();
    loadOptions.hasTiff = true;
    loadOptions.tiff.hasDirectoryIndex = true;
    loadOptions.tiff.directoryIndex = 0u;

    rawgl::HostImageData loaded;
    if (!load_image_with_options(path, loadOptions, loaded, "TIFF typed matrix")) {
        return false;
    }

    return verify_lossless_round_trip(loaded, source, "TIFF typed matrix");
}

static bool
verify_openexr_typed_options(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_f32_rgb_image(9, 7);

    rawgl::io::OpenExrSaveOptions openExr;
    openExr.hasCompression = true;
    openExr.compression = rawgl::io::OpenExrCompressionMode::Zip;
    openExr.hasLayout = true;
    openExr.layout = rawgl::io::OpenExrStorageLayout::Tiled;
    openExr.hasTileWidth = true;
    openExr.tileWidth = 8u;
    openExr.hasTileHeight = true;
    openExr.tileHeight = 8u;
    openExr.hasLineOrder = true;
    openExr.lineOrder = rawgl::io::OpenExrLineOrder::IncreasingY;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasOpenExr = true;
    codecOptions.openExr = openExr;

    if (!save_image_with_options(path,
                                 source,
                                 32,
                                 { { "openexr:compression", "not_a_codec" },
                                   { "openexr:layout", "scanlines" },
                                   { "openexr:line_order", "sideways" } },
                                 codecOptions,
                                 "OpenEXR typed matrix")) {
        return false;
    }
    if (!verify_openexr_tiled_layout(path)) {
        return false;
    }

    rawgl::io::ImageCodecLoadOptions loadOptions = make_native_load_options();
    loadOptions.hasOpenExr = true;
    loadOptions.openExr.hasChannelSelection = true;
    loadOptions.openExr.channelSelection = rawgl::io::OpenExrChannelSelection::Rgb;

    rawgl::HostImageData loaded;
    if (!load_image_with_options(path, loadOptions, loaded, "OpenEXR typed matrix")) {
        return false;
    }

    return verify_lossless_round_trip(loaded, source, "OpenEXR typed matrix");
}

int
main()
{
    const std::filesystem::path pngPath = "tests/outputs/rawgl_io_typed_matrix_u16.png";
    const std::filesystem::path jpegPath = "tests/outputs/rawgl_io_typed_matrix_progressive.jpg";
    const std::filesystem::path tiffPath = "tests/outputs/rawgl_io_typed_matrix_tiled.tif";
    const std::filesystem::path exrPath = "tests/outputs/rawgl_io_typed_matrix_tiled.exr";

    std::error_code removeError;
    std::filesystem::remove(pngPath, removeError);
    std::filesystem::remove(jpegPath, removeError);
    std::filesystem::remove(tiffPath, removeError);
    std::filesystem::remove(exrPath, removeError);

    if (!verify_png_typed_options(pngPath)) {
        return 1;
    }
    if (!verify_jpeg_typed_options(jpegPath)) {
        return 1;
    }
    if (!verify_tiff_typed_options(tiffPath)) {
        return 1;
    }
    if (!verify_openexr_typed_options(exrPath)) {
        return 1;
    }

    return 0;
}
