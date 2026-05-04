// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include "output_writer.h"
#include "texture_loader.h"

#include <OpenImageIO/oiioversion.h>

#include <exception>
#include <map>
#include <sstream>
#include <utility>

#if defined(RAWGL_HAS_LIBJPEG)
#    include <jpeglib.h>
#endif
#if defined(RAWGL_HAS_LIBPNG)
#    include <png.h>
#endif
#if defined(RAWGL_HAS_LIBTIFF)
#    include <tiffio.h>
#endif
#if defined(RAWGL_HAS_OPENEXR)
#    include <OpenEXR/ImfCompression.h>
#    include <OpenEXR/OpenEXRConfig.h>
#endif

#define RAWGL_STRINGIFY_DETAIL(value) #value
#define RAWGL_STRINGIFY(value) RAWGL_STRINGIFY_DETAIL(value)

namespace rawgl::io {
namespace {

static std::map<std::string, std::string>
to_attribute_map(const std::vector<Attribute>& attributes)
{
    std::map<std::string, std::string> result;
    for (const Attribute& attribute : attributes) {
        result.insert({ attribute.name, attribute.value });
    }
    return result;
}

static const char*
to_attribute_value(const bool value)
{
    return value ? "true" : "false";
}

static const char*
to_attribute_value(const ImageLoadBackendPolicy backendPolicy)
{
    switch (backendPolicy) {
        case ImageLoadBackendPolicy::Auto:
            return "auto";
        case ImageLoadBackendPolicy::NativeOnly:
            return "native";
        case ImageLoadBackendPolicy::OpenImageIoOnly:
            return "openimageio";
    }

    return "auto";
}

static const char*
to_attribute_value(const JpegLoadColorTransform colorTransform)
{
    switch (colorTransform) {
        case JpegLoadColorTransform::Auto:
            return "auto";
        case JpegLoadColorTransform::Grayscale:
            return "grayscale";
        case JpegLoadColorTransform::Rgb:
            return "rgb";
    }

    return "auto";
}

static const char*
to_attribute_value(const OpenExrChannelSelection channelSelection)
{
    switch (channelSelection) {
        case OpenExrChannelSelection::Auto:
            return "auto";
        case OpenExrChannelSelection::Luminance:
            return "luminance";
        case OpenExrChannelSelection::Rgb:
            return "rgb";
        case OpenExrChannelSelection::Rgba:
            return "rgba";
        case OpenExrChannelSelection::All:
            return "all";
    }

    return "auto";
}

static const char*
to_attribute_value(const JpegChromaSubsampling subsampling)
{
    switch (subsampling) {
        case JpegChromaSubsampling::Default:
            return "default";
        case JpegChromaSubsampling::S444:
            return "444";
        case JpegChromaSubsampling::S422:
            return "422";
        case JpegChromaSubsampling::S420:
            return "420";
        case JpegChromaSubsampling::S440:
            return "440";
        case JpegChromaSubsampling::S411:
            return "411";
    }

    return "default";
}

static const char*
to_attribute_value(const TiffCompressionMode compression)
{
    switch (compression) {
        case TiffCompressionMode::None:
            return "none";
        case TiffCompressionMode::Lzw:
            return "lzw";
        case TiffCompressionMode::PackBits:
            return "packbits";
        case TiffCompressionMode::Deflate:
            return "deflate";
        case TiffCompressionMode::AdobeDeflate:
            return "adobe_deflate";
        case TiffCompressionMode::Jpeg:
            return "jpeg";
        case TiffCompressionMode::Lzma:
            return "lzma";
        case TiffCompressionMode::Zstd:
            return "zstd";
        case TiffCompressionMode::Webp:
            return "webp";
        case TiffCompressionMode::Jxl:
            return "jxl";
        case TiffCompressionMode::JxlDng:
            return "jxl_dng";
        case TiffCompressionMode::Lerc:
            return "lerc";
    }

    return "none";
}

static const char*
to_attribute_value(const TiffPredictorMode predictor)
{
    switch (predictor) {
        case TiffPredictorMode::None:
            return "none";
        case TiffPredictorMode::Horizontal:
            return "horizontal";
        case TiffPredictorMode::Float:
            return "float";
    }

    return "none";
}

static const char*
to_attribute_value(const TiffStorageLayout layout)
{
    switch (layout) {
        case TiffStorageLayout::Strips:
            return "strips";
        case TiffStorageLayout::Tiled:
            return "tiled";
    }

    return "strips";
}

static const char*
to_attribute_value(const OpenExrCompressionMode compression)
{
    switch (compression) {
        case OpenExrCompressionMode::None:
            return "none";
        case OpenExrCompressionMode::Rle:
            return "rle";
        case OpenExrCompressionMode::Zips:
            return "zips";
        case OpenExrCompressionMode::Zip:
            return "zip";
        case OpenExrCompressionMode::Piz:
            return "piz";
        case OpenExrCompressionMode::Pxr24:
            return "pxr24";
        case OpenExrCompressionMode::B44:
            return "b44";
        case OpenExrCompressionMode::B44A:
            return "b44a";
        case OpenExrCompressionMode::Dwaa:
            return "dwaa";
        case OpenExrCompressionMode::Dwab:
            return "dwab";
        case OpenExrCompressionMode::Htj2k256:
            return "htj2k256";
        case OpenExrCompressionMode::Htj2k32:
            return "htj2k32";
    }

    return "zip";
}

static const char*
to_attribute_value(const OpenExrStorageLayout layout)
{
    switch (layout) {
        case OpenExrStorageLayout::Scanlines:
            return "scanlines";
        case OpenExrStorageLayout::Tiled:
            return "tiled";
    }

    return "scanlines";
}

static const char*
to_attribute_value(const OpenExrLineOrder lineOrder)
{
    switch (lineOrder) {
        case OpenExrLineOrder::IncreasingY:
            return "increasing_y";
        case OpenExrLineOrder::DecreasingY:
            return "decreasing_y";
        case OpenExrLineOrder::RandomY:
            return "random_y";
    }

    return "increasing_y";
}

static void
apply_jpeg_load_options(std::map<std::string, std::string>& attributes, const JpegLoadOptions& options)
{
    if (options.hasColorTransform) {
        attributes["jpeg:color_transform"] = to_attribute_value(options.colorTransform);
    }
}

static void
apply_png_load_options(std::map<std::string, std::string>& attributes, const PngLoadOptions& options)
{
    if (options.hasExpandTransparency) {
        attributes["png:expand_transparency"] = to_attribute_value(options.expandTransparency);
    }
}

static void
apply_tiff_load_options(std::map<std::string, std::string>& attributes, const TiffLoadOptions& options)
{
    if (options.hasDirectoryIndex) {
        attributes["tiff:directory_index"] = std::to_string(options.directoryIndex);
    }
}

static void
apply_openexr_load_options(std::map<std::string, std::string>& attributes, const OpenExrLoadOptions& options)
{
    if (options.hasChannelSelection) {
        attributes["openexr:channel_selection"] = to_attribute_value(options.channelSelection);
    }
}

static std::map<std::string, std::string>
to_load_attribute_map(const ImageLoadRequest& request)
{
    std::map<std::string, std::string> result = to_attribute_map(request.attributes);
    if (request.codecOptions.hasBackendPolicy) {
        result["rawgl:load_backend"] = to_attribute_value(request.codecOptions.backendPolicy);
    }
    if (request.codecOptions.hasJpeg) {
        apply_jpeg_load_options(result, request.codecOptions.jpeg);
    }
    if (request.codecOptions.hasPng) {
        apply_png_load_options(result, request.codecOptions.png);
    }
    if (request.codecOptions.hasTiff) {
        apply_tiff_load_options(result, request.codecOptions.tiff);
    }
    if (request.codecOptions.hasOpenExr) {
        apply_openexr_load_options(result, request.codecOptions.openExr);
    }
    return result;
}

static void
apply_jpeg_codec_options(std::map<std::string, std::string>& attributes, const JpegSaveOptions& options)
{
    if (options.hasQuality) {
        attributes["jpeg:quality"] = std::to_string(options.quality);
    }
    if (options.hasProgressive) {
        attributes["jpeg:progressive"] = to_attribute_value(options.progressive);
    }
    if (options.hasOptimize) {
        attributes["jpeg:optimize"] = to_attribute_value(options.optimize);
    }
    if (options.hasSubsampling) {
        attributes["jpeg:subsampling"] = to_attribute_value(options.subsampling);
    }
}

static void
apply_png_codec_options(std::map<std::string, std::string>& attributes, const PngSaveOptions& options)
{
    if (options.hasCompressionLevel) {
        attributes["png:compression_level"] = std::to_string(options.compressionLevel);
    }
    if (options.hasInterlaced) {
        attributes["png:interlace"] = to_attribute_value(options.interlaced);
    }
}

static void
apply_tiff_codec_options(std::map<std::string, std::string>& attributes, const TiffSaveOptions& options)
{
    if (options.hasCompression) {
        attributes["tiff:compression"] = to_attribute_value(options.compression);
    }
    if (options.hasPredictor) {
        attributes["tiff:predictor"] = to_attribute_value(options.predictor);
    }
    if (options.hasLayout) {
        attributes["tiff:layout"] = to_attribute_value(options.layout);
    }
    if (options.hasForceBigTiff) {
        attributes["tiff:big_tiff"] = to_attribute_value(options.forceBigTiff);
    }
    if (options.hasUnassociatedAlpha) {
        attributes["oiio:UnassociatedAlpha"] = to_attribute_value(options.unassociatedAlpha);
    }
    if (options.hasRowsPerStrip) {
        attributes["tiff:rows_per_strip"] = std::to_string(options.rowsPerStrip);
    }
    if (options.hasTileWidth) {
        attributes["tiff:tile_width"] = std::to_string(options.tileWidth);
    }
    if (options.hasTileHeight) {
        attributes["tiff:tile_height"] = std::to_string(options.tileHeight);
    }
    if (options.hasJpegQuality) {
        attributes["tiff:jpeg_quality"] = std::to_string(options.jpegQuality);
    }
    if (options.hasDeflateLevel) {
        attributes["tiff:zip_level"] = std::to_string(options.deflateLevel);
    }
    if (options.hasZstdLevel) {
        attributes["tiff:zstd_level"] = std::to_string(options.zstdLevel);
    }
    if (options.hasLzmaPreset) {
        attributes["tiff:lzma_preset"] = std::to_string(options.lzmaPreset);
    }
    if (options.hasWebpLevel) {
        attributes["tiff:webp_level"] = std::to_string(options.webpLevel);
    }
    if (options.hasWebpLossless) {
        attributes["tiff:webp_lossless"] = to_attribute_value(options.webpLossless);
    }
    if (options.hasWebpLosslessExact) {
        attributes["tiff:webp_lossless_exact"] = to_attribute_value(options.webpLosslessExact);
    }
}

static void
apply_openexr_codec_options(std::map<std::string, std::string>& attributes, const OpenExrSaveOptions& options)
{
    if (options.hasCompression) {
        attributes["openexr:compression"] = to_attribute_value(options.compression);
    }
    if (options.hasLayout) {
        attributes["openexr:layout"] = to_attribute_value(options.layout);
    }
    if (options.hasTileWidth) {
        attributes["openexr:tile_width"] = std::to_string(options.tileWidth);
    }
    if (options.hasTileHeight) {
        attributes["openexr:tile_height"] = std::to_string(options.tileHeight);
    }
    if (options.hasLineOrder) {
        attributes["openexr:line_order"] = to_attribute_value(options.lineOrder);
    }
    if (options.hasDwaCompressionLevel) {
        attributes["openexr:dwa_compression_level"] = std::to_string(options.dwaCompressionLevel);
    }
}

static std::map<std::string, std::string>
to_save_attribute_map(const ImageSaveRequest& request)
{
    std::map<std::string, std::string> result = to_attribute_map(request.attributes);
    if (request.codecOptions.hasJpeg) {
        apply_jpeg_codec_options(result, request.codecOptions.jpeg);
    }
    if (request.codecOptions.hasPng) {
        apply_png_codec_options(result, request.codecOptions.png);
    }
    if (request.codecOptions.hasTiff) {
        apply_tiff_codec_options(result, request.codecOptions.tiff);
    }
    if (request.codecOptions.hasOpenExr) {
        apply_openexr_codec_options(result, request.codecOptions.openExr);
    }
    return result;
}

static std::string
build_addressed_output_name(const FileOutputBinding& output)
{
    if (!output.usesArrayElement) {
        return output.name;
    }

    std::ostringstream stream;
    stream << output.name << "[" << output.arrayElement << "]";
    return stream.str();
}

static std::string
build_output_capture_key(const OutputSaveBinding& outputSave)
{
    std::ostringstream stream;
    stream << build_addressed_output_name(outputSave.output) << "::" << outputSave.output.passIndex;
    return stream.str();
}

static void
add_detail(ImageCodecCapabilities& capabilities, const char* name, const char* value)
{
    ImageIoCapabilityDetail detail;
    detail.name = name;
    detail.value = value;
    capabilities.details.push_back(std::move(detail));
}

static void
add_detail(ImageCodecCapabilities& capabilities, const char* name, const std::string& value)
{
    ImageIoCapabilityDetail detail;
    detail.name = name;
    detail.value = value;
    capabilities.details.push_back(std::move(detail));
}

static ImageCodecCapabilities
make_codec_base(const char* name)
{
    ImageCodecCapabilities codec;
    codec.name = name;
    codec.fallbackRead = true;
    codec.fallbackWrite = true;
    return codec;
}

static void
add_tiff_compression_mode(ImageCodecCapabilities& codec, const char* name, const bool configured)
{
    if (configured) {
        codec.nativeWriteCompressionModes.push_back(name);
    } else {
        codec.unavailableNativeWriteCompressionModes.push_back(name);
    }
}

static void
add_tiff_codec_modes(ImageCodecCapabilities& codec)
{
#if defined(RAWGL_HAS_LIBTIFF)
    add_tiff_compression_mode(codec, "none", true);
    add_tiff_compression_mode(codec, "lzw", TIFFIsCODECConfigured(COMPRESSION_LZW) != 0);
    add_tiff_compression_mode(codec, "packbits", TIFFIsCODECConfigured(COMPRESSION_PACKBITS) != 0);
#    if defined(COMPRESSION_JPEG)
    add_tiff_compression_mode(codec, "jpeg", TIFFIsCODECConfigured(COMPRESSION_JPEG) != 0);
#    endif
#    if defined(COMPRESSION_ADOBE_DEFLATE)
    add_tiff_compression_mode(codec, "deflate", TIFFIsCODECConfigured(COMPRESSION_ADOBE_DEFLATE) != 0);
    add_tiff_compression_mode(codec, "adobe_deflate", TIFFIsCODECConfigured(COMPRESSION_ADOBE_DEFLATE) != 0);
#    else
    add_tiff_compression_mode(codec, "deflate", TIFFIsCODECConfigured(COMPRESSION_DEFLATE) != 0);
#    endif
#    if defined(COMPRESSION_LZMA)
    add_tiff_compression_mode(codec, "lzma", TIFFIsCODECConfigured(COMPRESSION_LZMA) != 0);
#    endif
#    if defined(COMPRESSION_ZSTD)
    add_tiff_compression_mode(codec, "zstd", TIFFIsCODECConfigured(COMPRESSION_ZSTD) != 0);
#    endif
#    if defined(COMPRESSION_WEBP)
    add_tiff_compression_mode(codec, "webp", TIFFIsCODECConfigured(COMPRESSION_WEBP) != 0);
#    endif
#    if defined(COMPRESSION_JXL)
    add_tiff_compression_mode(codec, "jxl", TIFFIsCODECConfigured(COMPRESSION_JXL) != 0);
#    endif
#    if defined(COMPRESSION_JXL_DNG_1_7)
    add_tiff_compression_mode(codec, "jxl_dng", TIFFIsCODECConfigured(COMPRESSION_JXL_DNG_1_7) != 0);
#    endif
#    if defined(COMPRESSION_LERC)
    add_tiff_compression_mode(codec, "lerc", TIFFIsCODECConfigured(COMPRESSION_LERC) != 0);
#    endif
#else
    add_tiff_compression_mode(codec, "none", false);
    add_tiff_compression_mode(codec, "lzw", false);
    add_tiff_compression_mode(codec, "packbits", false);
    add_tiff_compression_mode(codec, "jpeg", false);
    add_tiff_compression_mode(codec, "deflate", false);
    add_tiff_compression_mode(codec, "adobe_deflate", false);
    add_tiff_compression_mode(codec, "lzma", false);
    add_tiff_compression_mode(codec, "zstd", false);
    add_tiff_compression_mode(codec, "webp", false);
    add_tiff_compression_mode(codec, "jxl", false);
    add_tiff_compression_mode(codec, "jxl_dng", false);
    add_tiff_compression_mode(codec, "lerc", false);
#endif
}

static void
add_openexr_compression_modes(ImageCodecCapabilities& codec)
{
#if defined(RAWGL_HAS_OPENEXR)
    for (int compressionIndex = 0; compressionIndex < static_cast<int>(OPENEXR_IMF_NAMESPACE::NUM_COMPRESSION_METHODS);
         ++compressionIndex) {
        const OPENEXR_IMF_NAMESPACE::Compression compression =
            static_cast<OPENEXR_IMF_NAMESPACE::Compression>(compressionIndex);
        if (!OPENEXR_IMF_NAMESPACE::isValidCompression(compressionIndex)) {
            continue;
        }
        std::string compressionName;
        OPENEXR_IMF_NAMESPACE::getCompressionNameFromId(compression, compressionName);
        if (!compressionName.empty()) {
            codec.nativeWriteCompressionModes.push_back(compressionName);
        }
    }
#else
    codec.unavailableNativeWriteCompressionModes.push_back("zip");
    codec.unavailableNativeWriteCompressionModes.push_back("zips");
    codec.unavailableNativeWriteCompressionModes.push_back("piz");
    codec.unavailableNativeWriteCompressionModes.push_back("rle");
    codec.unavailableNativeWriteCompressionModes.push_back("dwaa");
    codec.unavailableNativeWriteCompressionModes.push_back("dwab");
#endif
}

static void
add_jpeg_capabilities(ImageIoCapabilities& result)
{
    ImageCodecCapabilities codec = make_codec_base("jpeg");
    codec.extensions = { "jpg", "jpeg", "jpe", "jfif", "jif", "jfi" };
#if defined(RAWGL_HAS_LIBJPEG)
    codec.nativeRead = true;
    codec.nativeWrite = true;
    codec.fallbackWrite = false;
    codec.nativeReadComponentTypes = { "u8" };
    codec.nativeWriteComponentTypes = { "u8" };
    codec.nativeReadOptions = { "rawgl:load_backend", "rawgl:decode_backend", "jpeg:color_transform" };
    codec.nativeWriteOptions = { "jpeg:quality", "jpeg:progressive", "jpeg:optimize", "jpeg:subsampling" };
    codec.nativeWriteCompressionModes = { "baseline", "progressive" };
    add_detail(codec, "libjpeg.enabled", "true");
#    if defined(LIBJPEG_TURBO_VERSION)
    add_detail(codec, "libjpeg_turbo.version", RAWGL_STRINGIFY(LIBJPEG_TURBO_VERSION));
#    endif
#    if defined(LIBJPEG_TURBO_VERSION_NUMBER)
    add_detail(codec, "libjpeg_turbo.version_number", std::to_string(LIBJPEG_TURBO_VERSION_NUMBER));
    add_detail(codec,
               "libjpeg_turbo.high_bit_depth_api",
               LIBJPEG_TURBO_VERSION_NUMBER >= 3000000 ? "true" : "false");
#    else
    add_detail(codec, "libjpeg_turbo.high_bit_depth_api", "unknown");
#    endif
#    if defined(JPEG_LIB_VERSION)
    add_detail(codec, "libjpeg.version", std::to_string(JPEG_LIB_VERSION));
#    endif
#    if defined(BITS_IN_JSAMPLE)
    add_detail(codec, "libjpeg.bits_in_jsample", std::to_string(BITS_IN_JSAMPLE));
#    endif
    add_detail(codec, "rawgl.high_bit_depth_jpeg", "false");
#else
    codec.nativeWrite = false;
    codec.fallbackWrite = true;
    add_detail(codec, "libjpeg.enabled", "false");
    add_detail(codec, "rawgl.high_bit_depth_jpeg", "false");
#endif
    result.codecs.push_back(std::move(codec));
}

static void
add_png_capabilities(ImageIoCapabilities& result)
{
    ImageCodecCapabilities codec = make_codec_base("png");
    codec.extensions = { "png" };
#if defined(RAWGL_HAS_LIBPNG)
    codec.nativeRead = true;
    codec.nativeWrite = true;
    codec.fallbackWrite = false;
    codec.nativeReadComponentTypes = { "u8", "u16" };
    codec.nativeWriteComponentTypes = { "u8", "u16" };
    codec.nativeReadOptions = { "rawgl:load_backend", "rawgl:decode_backend", "png:expand_transparency" };
    codec.nativeWriteOptions = { "png:compressionLevel", "png:compression_level", "png:interlace", "png:interlaced" };
    codec.nativeWriteCompressionModes = { "zlib" };
    add_detail(codec, "libpng.enabled", "true");
    add_detail(codec, "libpng.version", PNG_LIBPNG_VER_STRING);
#else
    codec.nativeWrite = false;
    codec.fallbackWrite = true;
    add_detail(codec, "libpng.enabled", "false");
#endif
    result.codecs.push_back(std::move(codec));
}

static void
add_tiff_capabilities(ImageIoCapabilities& result)
{
    ImageCodecCapabilities codec = make_codec_base("tiff");
    codec.extensions = { "tif", "tiff", "tx", "env", "sm", "vsm" };
#if defined(RAWGL_HAS_LIBTIFF)
    codec.nativeRead = true;
    codec.nativeWrite = true;
    codec.fallbackWrite = false;
    codec.nativeReadComponentTypes = { "u8", "u16", "u32", "f32" };
    codec.nativeWriteComponentTypes = { "u8", "u16", "f32" };
    codec.nativeReadOptions = { "rawgl:load_backend", "rawgl:decode_backend", "tiff:directory_index",
                                "tiff:directoryIndex", "tiff:subimage" };
    codec.nativeWriteOptions = { "tiff:compression", "tiff:predictor", "tiff:layout", "tiff:tiled",
                                 "tiff:tile_width", "tiff:tile_height", "tiff:rows_per_strip",
                                 "tiff:big_tiff", "tiff:zip_level", "tiff:jpeg_quality",
                                 "tiff:zstd_level", "tiff:lzma_preset", "tiff:webp_level",
                                 "tiff:webp_lossless", "tiff:webp_lossless_exact" };
    add_detail(codec, "libtiff.enabled", "true");
    add_detail(codec, "libtiff.version", TIFFGetVersion());
#else
    codec.nativeWrite = false;
    codec.fallbackWrite = true;
    add_detail(codec, "libtiff.enabled", "false");
#endif
    add_tiff_codec_modes(codec);
    result.codecs.push_back(std::move(codec));
}

static void
add_openexr_capabilities(ImageIoCapabilities& result)
{
    ImageCodecCapabilities codec = make_codec_base("openexr");
    codec.extensions = { "exr" };
#if defined(RAWGL_HAS_OPENEXR)
    codec.nativeRead = true;
    codec.nativeWrite = true;
    codec.fallbackWrite = false;
    codec.nativeReadComponentTypes = { "f16", "f32", "u32" };
    codec.nativeWriteComponentTypes = { "f16", "f32" };
    codec.nativeReadOptions = { "rawgl:load_backend", "rawgl:decode_backend",
                                "openexr:channel_selection", "openexr:channelSelection" };
    codec.nativeWriteOptions = { "openexr:compression", "openexr:layout", "openexr:tiled",
                                 "openexr:tile_width", "openexr:tile_height", "openexr:line_order",
                                 "openexr:dwa_compression_level", "openexr:attribute:string:<name>" };
    add_detail(codec, "openexr.enabled", "true");
#    if defined(OPENEXR_VERSION_STRING)
    add_detail(codec, "openexr.version", OPENEXR_VERSION_STRING);
#    endif
    add_detail(codec, "openexr.tiled_one_level", "true");
    add_detail(codec, "openexr.multipart", "false");
#else
    codec.nativeWrite = false;
    codec.fallbackWrite = true;
    add_detail(codec, "openexr.enabled", "false");
#endif
    add_openexr_compression_modes(codec);
    result.codecs.push_back(std::move(codec));
}

static void
add_fallback_only_codec(ImageIoCapabilities& result,
                        const char* name,
                        const std::vector<std::string>& extensions)
{
    ImageCodecCapabilities codec = make_codec_base(name);
    codec.extensions = extensions;
    codec.nativeRead = false;
    codec.nativeWrite = false;
    codec.fallbackRead = true;
    codec.fallbackWrite = true;
    add_detail(codec, "backend", "OpenImageIO fallback");
    result.codecs.push_back(std::move(codec));
}

static ImageSaveResult
save_image_file_impl(const ImageSaveRequest& request)
{
    ImageSaveResult result;

    try {
        OutputWriteRequest writeRequest;
        writeRequest.path         = request.path;
        writeRequest.attributes   = to_save_attribute_map(request);
        writeRequest.alphaChannel = request.alphaChannel;
        writeRequest.bits         = request.bits;
        writeRequest.image        = &request.image;
        if (!save_image_output(writeRequest, result.errorMessage)) {
            return result;
        }
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

static ImageLoadResult
load_image_file_impl(const ImageLoadRequest& request)
{
    ImageLoadResult result;

    try {
        result.image   = load_host_image_data(request.path, to_load_attribute_map(request));
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

}  // namespace

IoRuntime::IoRuntime(const IoRuntimeOptions& options)
    : m_options(options)
{
}

ImageLoadResult
IoRuntime::loadImageFile(const ImageLoadRequest& request) const
{
    return load_image_file_impl(request);
}

ImageSaveResult
IoRuntime::saveImageFile(const ImageSaveRequest& request) const
{
    return save_image_file_impl(request);
}

WorkflowMaterializationResult
IoRuntime::materializeWorkflow(const Workflow& workflow,
                               const std::vector<FileInputBinding>& fileInputs,
                               const std::vector<FileOutputBinding>& fileOutputs) const
{
    WorkflowMaterializationResult result;
    result.workflow = workflow;

    try {
        for (const FileInputBinding& fileInput : fileInputs) {
            if (fileInput.passIndex >= result.workflow.passes.size()) {
                result.errorMessage = "file input references an out-of-range pass index";
                return result;
            }

            ImageLoadRequest loadRequest;
            loadRequest.path = fileInput.path;
            loadRequest.attributes = fileInput.attributes;
            loadRequest.codecOptions = fileInput.codecOptions;
            const ImageLoadResult loadResult = loadImageFile(loadRequest);
            if (!loadResult.success) {
                result.errorMessage = loadResult.errorMessage.empty() ? "workflow input materialization failed"
                                                                     : loadResult.errorMessage;
                return result;
            }

            Pass& pass = result.workflow.passes[fileInput.passIndex];
            for (const InputBinding& existingInput : pass.inputs) {
                if (existingInput.name != fileInput.name || existingInput.usesArrayElement != fileInput.usesArrayElement
                    || existingInput.arrayElement != fileInput.arrayElement) {
                    continue;
                }

                result.errorMessage = "file input duplicates an in-memory workflow input";
                return result;
            }

            InputBinding input;
            input.name = fileInput.name;
            input.sourceKind = InputSourceKind::hostTexture;
            input.attributes = fileInput.attributes;
            input.usesArrayElement = fileInput.usesArrayElement;
            input.arrayElement = fileInput.arrayElement;
            input.hostTexture = std::make_shared<HostImageData>(loadResult.image);
            pass.inputs.push_back(std::move(input));
        }

        for (const FileOutputBinding& fileOutput : fileOutputs) {
            if (fileOutput.passIndex >= result.workflow.passes.size()) {
                result.errorMessage = "file output references an out-of-range pass index";
                return result;
            }

            Pass& pass = result.workflow.passes[fileOutput.passIndex];
            bool matchedOutput = false;
            for (OutputBinding& output : pass.outputs) {
                if (output.name != fileOutput.name || output.usesArrayElement != fileOutput.usesArrayElement
                    || output.arrayElement != fileOutput.arrayElement) {
                    continue;
                }

                output.captureToHost = true;
                matchedOutput = true;
                break;
            }

            if (!matchedOutput) {
                OutputBinding output;
                output.name = fileOutput.name;
                output.format = fileOutput.format;
                output.channels = fileOutput.channels;
                output.alphaChannel = fileOutput.alphaChannel;
                output.bits = fileOutput.bits;
                output.attributes = fileOutput.attributes;
                output.captureToHost = true;
                output.usesArrayElement = fileOutput.usesArrayElement;
                output.arrayElement = fileOutput.arrayElement;
                pass.outputs.push_back(std::move(output));
            }

            result.outputSaves.push_back(OutputSaveBinding { fileOutput });
        }
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
        return result;
    }

    result.success = true;
    return result;
}

RunSettingsMaterializationResult
IoRuntime::materializeRunSettings(const RunRequest& request) const
{
    RunSettingsMaterializationResult result;
    result.settings = request.settings;

    try {
        for (const FileInputOverride& fileInput : request.fileInputs) {
            ImageLoadRequest loadRequest;
            loadRequest.path = fileInput.path;
            loadRequest.attributes = fileInput.attributes;
            loadRequest.codecOptions = fileInput.codecOptions;
            const ImageLoadResult loadResult = loadImageFile(loadRequest);
            if (!loadResult.success) {
                result.errorMessage = loadResult.errorMessage.empty() ? "run settings materialization failed"
                                                                     : loadResult.errorMessage;
                return result;
            }

            for (const InputOverride& existingOverride : result.settings.overrides) {
                if (existingOverride.passIndex != fileInput.passIndex || existingOverride.name != fileInput.name
                    || existingOverride.usesArrayElement != fileInput.usesArrayElement
                    || existingOverride.arrayElement != fileInput.arrayElement) {
                    continue;
                }

                result.errorMessage = "file input override duplicates an in-memory run override";
                return result;
            }

            InputOverride inputOverride;
            inputOverride.passIndex = fileInput.passIndex;
            inputOverride.name = fileInput.name;
            inputOverride.sourceKind = InputSourceKind::hostTexture;
            inputOverride.attributes = fileInput.attributes;
            inputOverride.usesArrayElement = fileInput.usesArrayElement;
            inputOverride.arrayElement = fileInput.arrayElement;
            inputOverride.hostTexture = std::make_shared<HostImageData>(loadResult.image);
            result.settings.overrides.push_back(std::move(inputOverride));
        }
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
        return result;
    }

    result.success = true;
    return result;
}

ImageSaveResult
IoRuntime::saveCapturedOutput(const OutputSaveBinding& outputSave, const RunResult& result) const
{
    ImageSaveResult saveResult;

    const auto captureIt = result.capturedOutputs.find(build_output_capture_key(outputSave));
    if (captureIt == result.capturedOutputs.end()) {
        saveResult.errorMessage = "captured output not found for save request";
        return saveResult;
    }

    ImageSaveRequest request;
    request.path         = outputSave.output.path;
    request.alphaChannel = outputSave.output.alphaChannel;
    request.bits         = outputSave.output.bits;
    request.image        = captureIt->second;
    request.codecOptions = outputSave.output.codecOptions;
    request.attributes.reserve(outputSave.output.attributes.size());
    for (const Attribute& attribute : outputSave.output.attributes) {
        request.attributes.push_back(attribute);
    }

    return saveImageFile(request);
}

SaveOutputsResult
IoRuntime::saveCapturedOutputs(const std::vector<OutputSaveBinding>& outputSaves, const RunResult& result) const
{
    SaveOutputsResult saveResult;

    for (const OutputSaveBinding& outputSave : outputSaves) {
        const ImageSaveResult singleSaveResult = saveCapturedOutput(outputSave, result);
        if (!singleSaveResult.success) {
            saveResult.errorMessage = singleSaveResult.errorMessage;
            return saveResult;
        }

        ++saveResult.savedCount;
    }

    saveResult.success = true;
    return saveResult;
}

ImageLoadResult
LoadImageFile(const ImageLoadRequest& request)
{
    return IoRuntime().loadImageFile(request);
}

ImageSaveResult
SaveImageFile(const ImageSaveRequest& request)
{
    return IoRuntime().saveImageFile(request);
}

ImageIoCapabilities
GetImageIoCapabilities()
{
    ImageIoCapabilities result;
    result.openImageIoFallback = true;

    add_jpeg_capabilities(result);
    add_png_capabilities(result);
    add_tiff_capabilities(result);
    add_openexr_capabilities(result);
    add_fallback_only_codec(result, "bmp", { "bmp" });
    add_fallback_only_codec(result, "tga", { "tga", "tpic" });
    add_fallback_only_codec(result, "hdr", { "hdr" });
    add_fallback_only_codec(result, "jpeg2000", { "jp2", "j2k" });
    add_fallback_only_codec(result, "webp", { "webp" });

    ImageCodecCapabilities fallback = make_codec_base("openimageio");
    fallback.nativeRead = false;
    fallback.nativeWrite = false;
    fallback.fallbackRead = true;
    fallback.fallbackWrite = true;
    fallback.extensions = { "*" };
    add_detail(fallback, "openimageio.version", OIIO_VERSION_STRING);
    result.codecs.push_back(std::move(fallback));

    return result;
}

}  // namespace rawgl::io

#undef RAWGL_STRINGIFY
#undef RAWGL_STRINGIFY_DETAIL
