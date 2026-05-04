// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "image_backend.h"

#include "gl_utils.h"
#include "exr_backend.h"
#include "image_io.h"
#include "jpg_backend.h"
#include "log.h"
#include "path_utils.h"
#include "png_backend.h"
#include "tiff_backend.h"
#include "timer.h"

#include <OpenImageIO/imageio.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace rawgl::io {
namespace {

enum class ImageBackendKind : uint8_t {
    OiioFallback,
    NativeJpegTurbo,
    NativePng,
    NativeTiff,
    NativeOpenExr,
};

enum class DecodeBackendPolicy : uint8_t {
    Auto,
    NativeOnly,
    OpenImageIoOnly,
};

static char
to_lower_ascii(const unsigned char c)
{
    return static_cast<char>(std::tolower(c));
}

static std::string
to_lower_copy(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), to_lower_ascii);
    return result;
}

static const std::string*
find_attribute_value(const std::map<std::string, std::string>& attributes, const char* key)
{
    const auto it = attributes.find(key);
    if (it == attributes.end()) {
        return nullptr;
    }
    return &it->second;
}

static bool
parse_decode_backend_policy(const std::map<std::string, std::string>& attributes,
                            DecodeBackendPolicy& policy,
                            std::string& errorMessage)
{
    policy = DecodeBackendPolicy::Auto;
    const std::string* attribute = find_attribute_value(attributes, "rawgl:load_backend");
    if (attribute == nullptr) {
        attribute = find_attribute_value(attributes, "rawgl:decode_backend");
    }
    if (attribute == nullptr) {
        return true;
    }

    const std::string normalized = to_lower_copy(*attribute);
    if (normalized == "auto") {
        policy = DecodeBackendPolicy::Auto;
        return true;
    }
    if (normalized == "native" || normalized == "native_only") {
        policy = DecodeBackendPolicy::NativeOnly;
        return true;
    }
    if (normalized == "oiio" || normalized == "openimageio" || normalized == "openimageio_only") {
        policy = DecodeBackendPolicy::OpenImageIoOnly;
        return true;
    }

    errorMessage = "unsupported image load backend policy";
    return false;
}

static ImageCodecFamily
get_image_codec_family_from_extension(const std::string& extension)
{
    if (extension == "bmp") {
        return ImageCodecFamily::Bmp;
    }
    if (extension == "png") {
        return ImageCodecFamily::Png;
    }
    if (extension == "jpg" || extension == "jpe" || extension == "jpeg" || extension == "jif" || extension == "jfif"
        || extension == "jfi") {
        return ImageCodecFamily::Jpeg;
    }
    if (extension == "tga" || extension == "tpic") {
        return ImageCodecFamily::Tga;
    }
    if (extension == "exr") {
        return ImageCodecFamily::Exr;
    }
    if (extension == "hdr") {
        return ImageCodecFamily::Hdr;
    }
    if (extension == "tif" || extension == "tiff" || extension == "tx" || extension == "env" || extension == "sm"
        || extension == "vsm") {
        return ImageCodecFamily::Tiff;
    }
    if (extension == "jp2" || extension == "j2k") {
        return ImageCodecFamily::Jpeg2000;
    }
    if (extension == "webp") {
        return ImageCodecFamily::Webp;
    }

    return ImageCodecFamily::Unknown;
}

static ImageBackendKind
select_decode_backend(const ImageCodecFamily codec)
{
    switch (codec) {
    case ImageCodecFamily::Jpeg:
#if defined(RAWGL_HAS_LIBJPEG)
        return ImageBackendKind::NativeJpegTurbo;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Png:
#if defined(RAWGL_HAS_LIBPNG)
        return ImageBackendKind::NativePng;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Tiff:
#if defined(RAWGL_HAS_LIBTIFF)
        return ImageBackendKind::NativeTiff;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Exr:
#if defined(RAWGL_HAS_OPENEXR)
        return ImageBackendKind::NativeOpenExr;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Unknown:
    case ImageCodecFamily::Bmp:
    case ImageCodecFamily::Tga:
    case ImageCodecFamily::Hdr:
    case ImageCodecFamily::Jpeg2000:
    case ImageCodecFamily::Webp:
    default:
        return ImageBackendKind::OiioFallback;
    }
}

static ImageBackendKind
select_encode_backend(const ImageCodecFamily codec)
{
    switch (codec) {
    case ImageCodecFamily::Jpeg:
#if defined(RAWGL_HAS_LIBJPEG)
        return ImageBackendKind::NativeJpegTurbo;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Png:
#if defined(RAWGL_HAS_LIBPNG)
        return ImageBackendKind::NativePng;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Tiff:
#if defined(RAWGL_HAS_LIBTIFF)
        return ImageBackendKind::NativeTiff;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Exr:
#if defined(RAWGL_HAS_OPENEXR)
        return ImageBackendKind::NativeOpenExr;
#else
        return ImageBackendKind::OiioFallback;
#endif
    case ImageCodecFamily::Unknown:
    case ImageCodecFamily::Bmp:
    case ImageCodecFamily::Tga:
    case ImageCodecFamily::Hdr:
    case ImageCodecFamily::Jpeg2000:
    case ImageCodecFamily::Webp:
    default:
        return ImageBackendKind::OiioFallback;
    }
}

static const char*
image_backend_kind_name(const ImageBackendKind backend)
{
    switch (backend) {
    case ImageBackendKind::OiioFallback: return "OpenImageIO";
    case ImageBackendKind::NativeJpegTurbo: return "native JPEG";
    case ImageBackendKind::NativePng: return "native PNG";
    case ImageBackendKind::NativeTiff: return "native TIFF";
    case ImageBackendKind::NativeOpenExr: return "native OpenEXR";
    }

    return "unknown";
}

static ImageComponentType
to_image_component_type(const OIIO::TypeDesc format)
{
    switch (format.basetype) {
    case OIIO::TypeDesc::UINT8: return ImageComponentType::U8;
    case OIIO::TypeDesc::UINT16: return ImageComponentType::U16;
    case OIIO::TypeDesc::UINT32: return ImageComponentType::U32;
    case OIIO::TypeDesc::HALF: return ImageComponentType::F16;
    case OIIO::TypeDesc::FLOAT: return ImageComponentType::F32;
    case OIIO::TypeDesc::DOUBLE: return ImageComponentType::F64;
    default: break;
    }

    return ImageComponentType::Unknown;
}

static bool
to_oiio_type_desc(const ImageComponentType componentType, OIIO::TypeDesc& format)
{
    switch (componentType) {
    case ImageComponentType::U8:
        format = OIIO::TypeDesc::UINT8;
        return true;
    case ImageComponentType::U16:
        format = OIIO::TypeDesc::UINT16;
        return true;
    case ImageComponentType::U32:
        format = OIIO::TypeDesc::UINT32;
        return true;
    case ImageComponentType::F16:
        format = OIIO::TypeDesc::HALF;
        return true;
    case ImageComponentType::F32:
        format = OIIO::TypeDesc::FLOAT;
        return true;
    case ImageComponentType::F64:
        format = OIIO::TypeDesc::DOUBLE;
        return true;
    case ImageComponentType::Unknown:
    default: break;
    }

    return false;
}

static bool
resolve_output_pixel_format(const unsigned int glType, OIIO::TypeDesc& pixelFormat)
{
    switch (glType) {
    case GL_UNSIGNED_BYTE:
        pixelFormat = OIIO::TypeDesc::UINT8;
        return true;
    case GL_UNSIGNED_SHORT:
        pixelFormat = OIIO::TypeDesc::UINT16;
        return true;
    case GL_UNSIGNED_INT:
        pixelFormat = OIIO::TypeDesc::UINT32;
        return true;
    case GL_HALF_FLOAT:
        pixelFormat = OIIO::TypeDesc::HALF;
        return true;
    case GL_FLOAT:
        pixelFormat = OIIO::TypeDesc::FLOAT;
        return true;
    default: break;
    }

    return false;
}

static DecodedImageData
decode_image_file_oiio(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    DecodedImageData result;
    void* pixelData = nullptr;
    OIIO::TypeDesc format;
    int width = 0;
    int height = 0;
    int channels = 0;
    int alphaChannel = -1;

    if (!image_utils::load_image(path, attributes, width, height, pixelData, channels, alphaChannel, format)) {
        result.errorMessage = OIIO::geterror();
        if (result.errorMessage.empty()) {
            result.errorMessage = "image decode failed";
        }
        return result;
    }

    result.componentType = to_image_component_type(format);
    if (result.componentType == ImageComponentType::Unknown) {
        if (pixelData != nullptr) {
            free(pixelData);
        }
        result.errorMessage = "unsupported decoded image component type";
        return result;
    }

    const size_t bytesPerComponent = byte_size_for_image_component(result.componentType);
    const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels)
                             * bytesPerComponent;

    result.width = width;
    result.height = height;
    result.channels = channels;
    result.alphaChannel = alphaChannel;
    result.bytes.resize(byteCount);

    if (pixelData != nullptr && byteCount > 0u) {
        memcpy(result.bytes.data(), pixelData, byteCount);
        free(pixelData);
    }

    result.success = true;
    return result;
}

static bool
encode_image_file_oiio(const std::string& path,
                       const std::map<std::string, std::string>& attributes,
                       int alphaChannel,
                       const HostImageData& image,
                       const ImageEncodeSettings& settings,
                       std::string& errorMessage)
{
    Timer timer;

    LOG(info) << "Saving image: " << path;

    std::unique_ptr<OIIO::ImageOutput> output = OIIO::ImageOutput::create(path);
    if (!output) {
        errorMessage = OIIO::geterror();
        if (errorMessage.empty()) {
            errorMessage = "can't create image output";
        }
        return false;
    }

    OIIO::TypeDesc outputFormat;
    if (!to_oiio_type_desc(settings.componentType, outputFormat)) {
        errorMessage = "unsupported output component type";
        return false;
    }

    OIIO::ImageSpec spec(image.width, image.height, image.channels, outputFormat);
    for (const auto& attribute : attributes) {
        spec.attribute(attribute.first, attribute.second);
    }

    spec.alpha_channel = alphaChannel >= 0 ? alphaChannel : image.alphaChannel;

    if (!output->open(path, spec)) {
        errorMessage = OIIO::geterror();
        if (errorMessage.empty()) {
            errorMessage = "can't open file for writing";
        }
        return false;
    }

    OIIO::TypeDesc pixelFormat;
    if (!resolve_output_pixel_format(image.glType, pixelFormat)) {
        errorMessage = "unsupported host image type for output";
        return false;
    }

    if (!output->write_image(pixelFormat,
                             image.bytes.data(),
                             OIIO::AutoStride,
                             OIIO::AutoStride,
                             OIIO::AutoStride,
                             (*image_utils::progress_callback))) {
        errorMessage = OIIO::geterror();
        if (errorMessage.empty()) {
            errorMessage = "can't write image file";
        }
        return false;
    }

    if (!output->close()) {
        errorMessage = OIIO::geterror();
        if (errorMessage.empty()) {
            errorMessage = "can't close image file after writing";
        }
        return false;
    }

    LOG(debug) << "Finished in " << timer.nowText();
    return true;
}

}  // namespace

ImageCodecFamily
get_image_codec_family(const std::string& path)
{
    return get_image_codec_family_from_extension(get_file_ext(path));
}

size_t
byte_size_for_image_component(const ImageComponentType componentType)
{
    switch (componentType) {
    case ImageComponentType::U8: return 1u;
    case ImageComponentType::U16:
    case ImageComponentType::F16: return 2u;
    case ImageComponentType::U32:
    case ImageComponentType::F32: return 4u;
    case ImageComponentType::F64: return 8u;
    case ImageComponentType::Unknown:
    default: break;
    }

    return 0u;
}

DecodedImageData
decode_image_file(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    DecodeBackendPolicy policy = DecodeBackendPolicy::Auto;
    std::string policyErrorMessage;
    if (!parse_decode_backend_policy(attributes, policy, policyErrorMessage)) {
        DecodedImageData result;
        result.errorMessage = policyErrorMessage;
        return result;
    }

    const ImageCodecFamily codec = get_image_codec_family(path);
    if (policy == DecodeBackendPolicy::OpenImageIoOnly) {
        LOG(debug) << "Image decode backend selected: OpenImageIO for " << path;
        return decode_image_file_oiio(path, attributes);
    }

    const ImageBackendKind backend = select_decode_backend(codec);
    LOG(debug) << "Image decode backend selected: " << image_backend_kind_name(backend) << " for " << path;

    switch (backend) {
    case ImageBackendKind::OiioFallback:
        if (policy == DecodeBackendPolicy::NativeOnly) {
            DecodedImageData result;
            result.errorMessage = "native decode backend is not available for this image family";
            return result;
        }
        return decode_image_file_oiio(path, attributes);
    case ImageBackendKind::NativeJpegTurbo: {
        DecodedImageData result = decode_jpg_file(path, attributes);
        if (result.success) {
            return result;
        }
        if (policy == DecodeBackendPolicy::NativeOnly) {
            return result;
        }
        LOG(warning) << "Native JPEG decode failed for " << path << ", falling back to OIIO: " << result.errorMessage;
        return decode_image_file_oiio(path, attributes);
    }
    case ImageBackendKind::NativePng: {
        DecodedImageData result = decode_png_file(path, attributes);
        if (result.success) {
            return result;
        }
        if (policy == DecodeBackendPolicy::NativeOnly) {
            return result;
        }
        LOG(warning) << "Native PNG decode failed for " << path << ", falling back to OIIO: " << result.errorMessage;
        return decode_image_file_oiio(path, attributes);
    }
    case ImageBackendKind::NativeTiff: {
        DecodedImageData result = decode_tiff_file(path, attributes);
        if (result.success) {
            return result;
        }
        if (policy == DecodeBackendPolicy::NativeOnly) {
            return result;
        }
        LOG(warning) << "Native TIFF decode failed for " << path << ", falling back to OIIO: " << result.errorMessage;
        return decode_image_file_oiio(path, attributes);
    }
    case ImageBackendKind::NativeOpenExr: {
        DecodedImageData result = decode_exr_file(path, attributes);
        if (result.success) {
            return result;
        }
        if (policy == DecodeBackendPolicy::NativeOnly) {
            return result;
        }
        LOG(warning) << "Native OpenEXR decode failed for " << path << ", falling back to OIIO: " << result.errorMessage;
        return decode_image_file_oiio(path, attributes);
    }
    default:
        return decode_image_file_oiio(path, attributes);
    }
}

ImageEncodeSettings
resolve_image_encode_settings(const std::string& path, int bits)
{
    ImageEncodeSettings settings;
    settings.codec = get_image_codec_family(path);

    bool defaulted = false;
    const OIIO::TypeDesc outputFormat = image_utils::get_output_format(path, bits, defaulted);
    settings.componentType = to_image_component_type(outputFormat);
    settings.defaulted = defaulted;
    return settings;
}

bool
encode_image_file(const std::string& path,
                  const std::map<std::string, std::string>& attributes,
                  int alphaChannel,
                  const HostImageData& image,
                  const ImageEncodeSettings& settings,
                  std::string& errorMessage)
{
    const ImageBackendKind backend = select_encode_backend(settings.codec);
    LOG(debug) << "Image encode backend selected: " << image_backend_kind_name(backend) << " for " << path;

    switch (backend) {
    case ImageBackendKind::OiioFallback:
        return encode_image_file_oiio(path, attributes, alphaChannel, image, settings, errorMessage);
    case ImageBackendKind::NativeJpegTurbo: {
        std::string nativeErrorMessage;
        if (encode_jpg_file(path, attributes, alphaChannel, image, nativeErrorMessage)) {
            return true;
        }
        errorMessage = nativeErrorMessage.empty() ? "native JPEG write failed" : nativeErrorMessage;
        return false;
    }
    case ImageBackendKind::NativePng: {
        std::string nativeErrorMessage;
        if (encode_png_file(path, attributes, alphaChannel, image, settings, nativeErrorMessage)) {
            return true;
        }
        errorMessage = nativeErrorMessage.empty() ? "native PNG write failed" : nativeErrorMessage;
        return false;
    }
    case ImageBackendKind::NativeTiff: {
        std::string nativeErrorMessage;
        if (encode_tiff_file(path, attributes, alphaChannel, image, settings, nativeErrorMessage)) {
            return true;
        }
        errorMessage = nativeErrorMessage.empty() ? "native TIFF write failed" : nativeErrorMessage;
        return false;
    }
    case ImageBackendKind::NativeOpenExr: {
        std::string nativeErrorMessage;
        if (encode_exr_file(path, attributes, alphaChannel, image, settings, nativeErrorMessage)) {
            return true;
        }
        errorMessage = nativeErrorMessage.empty() ? "native OpenEXR write failed" : nativeErrorMessage;
        return false;
    }
    default:
        return encode_image_file_oiio(path, attributes, alphaChannel, image, settings, errorMessage);
    }
}

}  // namespace rawgl::io
