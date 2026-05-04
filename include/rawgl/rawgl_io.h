// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <rawgl/rawgl.h>

namespace rawgl::io {

struct MetadataDocumentStorage;

/// Controls CPU-side image decode and encode worker policy.
struct IoRuntimeOptions {
    int decodeWorkerCount = 0;
    int encodeWorkerCount = 0;
};

/// Decode backend policy for file-backed image loads.
enum class ImageLoadBackendPolicy : uint8_t {
    Auto,
    NativeOnly,
    OpenImageIoOnly,
};

/// JPEG output color transform for native JPEG loads.
enum class JpegLoadColorTransform : uint8_t {
    Auto,
    Grayscale,
    Rgb,
};

/// Typed native JPEG reader options.
struct JpegLoadOptions {
    bool hasColorTransform = false;
    JpegLoadColorTransform colorTransform = JpegLoadColorTransform::Auto;
};

/// Typed native PNG reader options.
struct PngLoadOptions {
    bool hasExpandTransparency = false;
    bool expandTransparency = true;
};

/// Typed native TIFF reader options.
struct TiffLoadOptions {
    bool hasDirectoryIndex = false;
    uint32_t directoryIndex = 0;
};

/// OpenEXR channel selection policy for native OpenEXR loads.
enum class OpenExrChannelSelection : uint8_t {
    Auto,
    Luminance,
    Rgb,
    Rgba,
    All,
};

/// Typed native OpenEXR reader options.
struct OpenExrLoadOptions {
    bool hasChannelSelection = false;
    OpenExrChannelSelection channelSelection = OpenExrChannelSelection::Auto;
};

/// Typed native codec reader options.
///
/// Legacy string \ref Attribute entries remain supported. When both typed
/// options and string attributes set the same native reader option, the typed
/// option wins.
struct ImageCodecLoadOptions {
    bool hasBackendPolicy = false;
    ImageLoadBackendPolicy backendPolicy = ImageLoadBackendPolicy::Auto;
    bool hasJpeg = false;
    JpegLoadOptions jpeg;
    bool hasPng = false;
    PngLoadOptions png;
    bool hasTiff = false;
    TiffLoadOptions tiff;
    bool hasOpenExr = false;
    OpenExrLoadOptions openExr;
};

/// Describes one file-backed image load request.
struct ImageLoadRequest {
    /// Source image path.
    std::string path;
    /// Compatibility loader attributes such as colorspace hints.
    std::vector<Attribute> attributes;
    /// Typed native codec-specific reader options.
    ImageCodecLoadOptions codecOptions;
};

/// Result of loading one file-backed image into host memory.
struct ImageLoadResult {
    /// False when loading failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Loaded host-memory image payload.
    HostImageData image;
};

/// JPEG chroma subsampling policy for native JPEG output.
enum class JpegChromaSubsampling : uint8_t {
    Default,
    S444,
    S422,
    S420,
    S440,
    S411,
};

/// Typed native JPEG writer options.
struct JpegSaveOptions {
    bool hasQuality = false;
    int quality = 95;
    bool hasProgressive = false;
    bool progressive = false;
    bool hasOptimize = false;
    bool optimize = false;
    bool hasSubsampling = false;
    JpegChromaSubsampling subsampling = JpegChromaSubsampling::Default;
};

/// Typed native PNG writer options.
struct PngSaveOptions {
    bool hasCompressionLevel = false;
    int compressionLevel = -1;
    bool hasInterlaced = false;
    bool interlaced = false;
};

/// TIFF compression policy for native TIFF output.
enum class TiffCompressionMode : uint8_t {
    None,
    Lzw,
    PackBits,
    Deflate,
    AdobeDeflate,
    Jpeg,
    Lzma,
    Zstd,
    Webp,
    Jxl,
    JxlDng,
    Lerc,
};

/// TIFF predictor policy for native TIFF output.
enum class TiffPredictorMode : uint8_t {
    None,
    Horizontal,
    Float,
};

/// TIFF storage layout for native TIFF output.
enum class TiffStorageLayout : uint8_t {
    Strips,
    Tiled,
};

/// Typed native TIFF writer options.
struct TiffSaveOptions {
    bool hasCompression = false;
    TiffCompressionMode compression = TiffCompressionMode::None;
    bool hasPredictor = false;
    TiffPredictorMode predictor = TiffPredictorMode::None;
    bool hasLayout = false;
    TiffStorageLayout layout = TiffStorageLayout::Strips;
    bool hasForceBigTiff = false;
    bool forceBigTiff = false;
    bool hasUnassociatedAlpha = false;
    bool unassociatedAlpha = false;
    bool hasRowsPerStrip = false;
    uint32_t rowsPerStrip = 0;
    bool hasTileWidth = false;
    uint32_t tileWidth = 0;
    bool hasTileHeight = false;
    uint32_t tileHeight = 0;
    bool hasJpegQuality = false;
    uint32_t jpegQuality = 0;
    bool hasDeflateLevel = false;
    uint32_t deflateLevel = 0;
    bool hasZstdLevel = false;
    uint32_t zstdLevel = 0;
    bool hasLzmaPreset = false;
    uint32_t lzmaPreset = 0;
    bool hasWebpLevel = false;
    uint32_t webpLevel = 0;
    bool hasWebpLossless = false;
    bool webpLossless = false;
    bool hasWebpLosslessExact = false;
    bool webpLosslessExact = false;
};

/// OpenEXR compression policy for native OpenEXR output.
enum class OpenExrCompressionMode : uint8_t {
    None,
    Rle,
    Zips,
    Zip,
    Piz,
    Pxr24,
    B44,
    B44A,
    Dwaa,
    Dwab,
    Htj2k256,
    Htj2k32,
};

/// OpenEXR storage layout for native OpenEXR output.
enum class OpenExrStorageLayout : uint8_t {
    Scanlines,
    Tiled,
};

/// OpenEXR line order policy for native OpenEXR output.
enum class OpenExrLineOrder : uint8_t {
    IncreasingY,
    DecreasingY,
    RandomY,
};

/// Typed native OpenEXR writer options.
struct OpenExrSaveOptions {
    bool hasCompression = false;
    OpenExrCompressionMode compression = OpenExrCompressionMode::Zip;
    bool hasLayout = false;
    OpenExrStorageLayout layout = OpenExrStorageLayout::Scanlines;
    bool hasTileWidth = false;
    uint32_t tileWidth = 0;
    bool hasTileHeight = false;
    uint32_t tileHeight = 0;
    bool hasLineOrder = false;
    OpenExrLineOrder lineOrder = OpenExrLineOrder::IncreasingY;
    bool hasDwaCompressionLevel = false;
    float dwaCompressionLevel = 0.0f;
};

/// Typed native codec writer options.
///
/// Legacy string \ref Attribute entries remain supported. When both typed
/// options and string attributes set the same native writer option, the typed
/// option wins.
struct ImageCodecSaveOptions {
    bool hasJpeg = false;
    JpegSaveOptions jpeg;
    bool hasPng = false;
    PngSaveOptions png;
    bool hasTiff = false;
    TiffSaveOptions tiff;
    bool hasOpenExr = false;
    OpenExrSaveOptions openExr;
};

/// Describes one file-backed image save request.
struct ImageSaveRequest {
    /// Destination image path.
    std::string path;
    /// Compatibility codec-specific writer hints.
    std::vector<Attribute> attributes;
    /// Typed native codec-specific writer options.
    ImageCodecSaveOptions codecOptions;
    /// Explicit alpha channel, or -1 to use \ref HostImageData::alphaChannel.
    int alphaChannel = -1;
    /// Preferred output bit depth when the target format supports it.
    int bits = 16;
    /// Source host-memory image payload.
    HostImageData image;
};

/// Result of saving one host-memory image to disk.
struct ImageSaveResult {
    /// False when saving failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
};

/// One name/value detail reported by the image IO capability query.
struct ImageIoCapabilityDetail {
    std::string name;
    std::string value;
};

/// Build-time and run-time capabilities for one image codec family.
struct ImageCodecCapabilities {
    /// Stable family name such as `jpeg`, `png`, `tiff`, `openexr`, or `webp`.
    std::string name;
    /// Common file extensions handled by this codec family.
    std::vector<std::string> extensions;
    /// True when RawGL routes decode through a native backend first.
    bool nativeRead = false;
    /// True when RawGL routes encode through a native backend first.
    bool nativeWrite = false;
    /// True when OpenImageIO fallback read is available for this family.
    bool fallbackRead = false;
    /// True when OpenImageIO fallback write is available for this family.
    bool fallbackWrite = false;
    /// Component types RawGL can currently return for this native reader.
    std::vector<std::string> nativeReadComponentTypes;
    /// Component types RawGL can currently write through this native writer.
    std::vector<std::string> nativeWriteComponentTypes;
    /// Attribute names accepted by RawGL's native reader for this family.
    std::vector<std::string> nativeReadOptions;
    /// Compression names accepted by RawGL's native writer for this family.
    std::vector<std::string> nativeWriteCompressionModes;
    /// Compression names known but unavailable in the current dependency build.
    std::vector<std::string> unavailableNativeWriteCompressionModes;
    /// Attribute names accepted by RawGL's native writer for this family.
    std::vector<std::string> nativeWriteOptions;
    /// Additional build/library details.
    std::vector<ImageIoCapabilityDetail> details;
};

/// Image IO capabilities for the current RawGL build.
struct ImageIoCapabilities {
    bool openImageIoFallback = false;
    std::vector<ImageCodecCapabilities> codecs;
};

/// Export name style for metadata readback.
enum class MetadataNameStyle : uint8_t {
    Canonical,
    XmpPortable,
    Oiio,
};

/// Export aliasing policy for metadata readback.
enum class MetadataNamePolicy : uint8_t {
    Spec,
    ExifToolAlias,
};

/// Metadata key family exposed by `rawgl::io`.
enum class MetadataKeyKind : uint8_t {
    ExifTag,
    Comment,
    ExrAttribute,
    IptcDataset,
    XmpProperty,
    IccHeaderField,
    IccTag,
    PhotoshopIrb,
    PhotoshopIrbField,
    GeotiffKey,
    PrintImField,
    BmffField,
    JumbfField,
    JumbfCborKey,
    PngText,
};

/// Top-level metadata value storage kind exposed by `rawgl::io`.
enum class MetadataValueKind : uint8_t {
    Empty,
    Scalar,
    Array,
    Bytes,
    Text,
};

/// Metadata scalar or array element type exposed by `rawgl::io`.
enum class MetadataElementType : uint8_t {
    U8,
    I8,
    U16,
    I16,
    U32,
    I32,
    U64,
    I64,
    F32,
    F64,
    URational,
    SRational,
};

/// Text encoding hint for metadata text payloads.
enum class MetadataTextEncoding : uint8_t {
    Unknown,
    Ascii,
    Utf8,
    Utf16LE,
    Utf16BE,
};

/// Coarse policy for transferring source metadata into a target image.
enum class MetadataTransferSafety : uint8_t {
    /// Preserve source camera/color metadata for compatible file repackaging or recompression.
    CompatibleFile,
    /// Drop source-specific RAW, ICC, MakerNote, and non-C2PA JUMBF data for rendered outputs.
    RenderedImage,
};

/// Per-entry metadata flags preserved from the decode layer.
enum class MetadataEntryFlags : uint32_t {
    None = 0,
    Deleted = 1U << 0U,
    Dirty = 1U << 1U,
    Derived = 1U << 2U,
    Truncated = 1U << 3U,
    Unreadable = 1U << 4U,
    ContextualName = 1U << 5U,
};

constexpr MetadataEntryFlags
operator|(MetadataEntryFlags a, MetadataEntryFlags b) noexcept
{
    return static_cast<MetadataEntryFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr MetadataEntryFlags
operator&(MetadataEntryFlags a, MetadataEntryFlags b) noexcept
{
    return static_cast<MetadataEntryFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

constexpr bool
any(MetadataEntryFlags flags, MetadataEntryFlags test) noexcept
{
    return static_cast<uint32_t>(flags & test) != 0U;
}

/// One exported metadata item with a best-effort text preview.
struct MetadataEntry {
    MetadataKeyKind keyKind = MetadataKeyKind::ExifTag;
    MetadataValueKind valueKind = MetadataValueKind::Empty;
    MetadataElementType elementType = MetadataElementType::U8;
    MetadataTextEncoding textEncoding = MetadataTextEncoding::Unknown;
    MetadataEntryFlags flags = MetadataEntryFlags::None;
    uint32_t count = 0;
    std::string name;
    std::string valueText;
};

/// One typed metadata value owned by RawGL.
struct MetadataValue {
    MetadataValueKind kind = MetadataValueKind::Empty;
    MetadataElementType elementType = MetadataElementType::U8;
    MetadataTextEncoding textEncoding = MetadataTextEncoding::Unknown;
    uint32_t count = 0;
    std::vector<std::byte> bytes;
};

/// One typed metadata field owned by RawGL.
struct MetadataField {
    MetadataKeyKind keyKind = MetadataKeyKind::ExifTag;
    MetadataEntryFlags flags = MetadataEntryFlags::None;
    std::string name;
    MetadataValue value;
};

/// Typed metadata document owned by RawGL.
struct MetadataDocument {
    std::vector<MetadataField> fields;
    /// Optional opaque backend-side cache used for transfer helpers.
    std::shared_ptr<const MetadataDocumentStorage> storage;
};

/// Describes one metadata read request.
struct MetadataReadRequest {
    /// Source image or container path.
    std::string path;
    /// Export naming style for MetadataEntry::name.
    MetadataNameStyle nameStyle = MetadataNameStyle::Canonical;
    /// Export aliasing policy for MetadataEntry::name.
    MetadataNamePolicy namePolicy = MetadataNamePolicy::Spec;
    /// Include MakerNote-derived metadata families when available.
    bool includeMakernotes = false;
    /// Maximum bytes used when previewing byte/text payloads.
    size_t maxValuePreviewBytes = 64;
    /// Maximum elements shown for scalar-array previews.
    size_t maxValuePreviewElements = 8;
};

/// Result of reading metadata from one file-backed image or container.
struct MetadataReadResult {
    /// False when metadata read failed or the file format is unsupported.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Exported metadata entries in source order.
    std::vector<MetadataEntry> entries;
};

/// Describes one typed metadata document read request.
struct MetadataDocumentReadRequest {
    /// Source image or container path.
    std::string path;
    /// Export naming style for MetadataField::name.
    MetadataNameStyle nameStyle = MetadataNameStyle::Canonical;
    /// Export aliasing policy for MetadataField::name.
    MetadataNamePolicy namePolicy = MetadataNamePolicy::Spec;
    /// Include MakerNote-derived metadata families when available.
    bool includeMakernotes = false;
};

/// Result of reading one typed metadata document.
struct MetadataDocumentReadResult {
    /// False when metadata read failed or the file format is unsupported.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Typed metadata document in source order.
    MetadataDocument document;
};

/// Describes one file-backed source-metadata transfer into an already-written image.
struct ImageMetadataTransferRequest {
    /// Target image path to update in place.
    std::string path;
    /// Source metadata document read from the original file/container.
    MetadataDocument sourceMetadata;
    /// Coarse transfer safety policy. RawGL-generated outputs default to rendered-image safety.
    MetadataTransferSafety safety = MetadataTransferSafety::RenderedImage;
    /// True when \ref targetImage contains the exact target pixel layout.
    bool hasTargetImage = false;
    /// Optional target image facts used to write target-correct image-layout metadata.
    HostImageData targetImage;
};

/// Result of transferring source metadata into a file-backed target image.
struct ImageMetadataTransferResult {
    /// False when the target format is unsupported or transfer failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
};

/// One file-backed workflow input owned by `rawgl::io`.
struct FileInputBinding {
    size_t passIndex = 0;
    std::string name;
    std::string path;
    std::vector<Attribute> attributes;
    ImageCodecLoadOptions codecOptions;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
};

/// One file-backed per-run input override owned by `rawgl::io`.
struct FileInputOverride {
    size_t passIndex = 0;
    std::string name;
    std::string path;
    std::vector<Attribute> attributes;
    ImageCodecLoadOptions codecOptions;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
};

/// One file-backed workflow output owned by `rawgl::io`.
struct FileOutputBinding {
    size_t passIndex = 0;
    std::string name;
    std::string path;
    std::string format = "rgb32f";
    int channels = 3;
    int alphaChannel = -1;
    int bits = 16;
    std::vector<Attribute> attributes;
    ImageCodecSaveOptions codecOptions;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
};

/// One deferred file-backed workflow output.
struct OutputSaveBinding {
    /// Original file-backed output declaration.
    FileOutputBinding output;
};

/// Result of materializing file-backed workflow inputs for in-memory execution.
struct WorkflowMaterializationResult {
    /// False when materialization failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Materialized workflow with file-backed inputs replaced by host textures.
    Workflow workflow;
    /// File-backed outputs split out from the workflow for later save.
    std::vector<OutputSaveBinding> outputSaves;
};

/// Result of materializing file-backed per-run input overrides.
struct RunSettingsMaterializationResult {
    /// False when materialization failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Materialized run settings with file-backed overrides replaced by host textures.
    RunSettings settings;
};

/// One IO-backed per-run request.
struct RunRequest {
    RunSettings settings;
    std::vector<FileInputOverride> fileInputs;
};

/// Result of saving one or more captured workflow outputs to files.
struct SaveOutputsResult {
    /// False when saving failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Number of outputs written before completion or failure.
    size_t savedCount = 0;
};

class PreparedIoWorkflow;

/// Result of preparing one IO-backed workflow for repeated execution.
struct PrepareWorkflowResult {
    /// False when preparation failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Prepared IO-backed workflow when \ref success is true.
    std::unique_ptr<PreparedIoWorkflow> workflow;
};

/// Builds one file-backed image input binding for use with \ref IoRuntime.
inline FileInputBinding
FileTextureInput(size_t passIndex,
                 std::string name,
                 std::string path,
                 std::vector<Attribute> attributes = {},
                 ImageCodecLoadOptions codecOptions = {})
{
    FileInputBinding result;
    result.passIndex = passIndex;
    result.name = std::move(name);
    result.path = std::move(path);
    result.attributes = std::move(attributes);
    result.codecOptions = codecOptions;
    return result;
}

/// Builds one file-backed image per-run override for use with \ref IoRuntime.
inline FileInputOverride
FileTextureOverride(size_t passIndex,
                    std::string name,
                    std::string path,
                    std::vector<Attribute> attributes = {},
                    ImageCodecLoadOptions codecOptions = {})
{
    FileInputOverride result;
    result.passIndex = passIndex;
    result.name = std::move(name);
    result.path = std::move(path);
    result.attributes = std::move(attributes);
    result.codecOptions = codecOptions;
    return result;
}

/// Builds one file-backed output binding for use with \ref IoRuntime.
inline FileOutputBinding
FileOutput(size_t passIndex,
           std::string name,
           std::string path,
           std::string format = "rgb32f",
           int channels = 3,
           int alphaChannel = -1,
           int bits = 16,
           std::vector<Attribute> attributes = {},
           ImageCodecSaveOptions codecOptions = {})
{
    FileOutputBinding result;
    result.passIndex = passIndex;
    result.name = std::move(name);
    result.path = std::move(path);
    result.format = std::move(format);
    result.channels = channels;
    result.alphaChannel = alphaChannel;
    result.bits = bits;
    result.attributes = std::move(attributes);
    result.codecOptions = codecOptions;
    return result;
}

/// Public IO runtime façade for file-backed inputs and outputs.
///
/// This is the preferred public translation layer for file-oriented workflows.
/// `rawgl_core` should stay host-memory oriented, while `IoRuntime` owns the
/// materialization of file-backed inputs and deferred output saves.
class IoRuntime {
public:
    explicit IoRuntime(const IoRuntimeOptions& options = {});

    const IoRuntimeOptions& options() const { return m_options; }

    /// Loads one file-backed image into \ref HostImageData.
    ImageLoadResult
    loadImageFile(const ImageLoadRequest& request) const;

    /// Saves one \ref HostImageData payload to a file-backed image.
    ImageSaveResult
    saveImageFile(const ImageSaveRequest& request) const;

    /// Reads metadata from one file-backed image or container.
    MetadataReadResult
    readMetadataFile(const MetadataReadRequest& request) const;

    /// Reads a typed metadata document from one file-backed image or container.
    MetadataDocumentReadResult
    readMetadataDocumentFile(const MetadataDocumentReadRequest& request) const;

    /// Transfers source metadata into an already-written file-backed target image.
    ImageMetadataTransferResult
    transferImageMetadataFile(const ImageMetadataTransferRequest& request) const;

    /// Rewrites file-backed workflow inputs and outputs into explicit host-memory workflow state.
    WorkflowMaterializationResult
    materializeWorkflow(const Workflow& workflow,
                        const std::vector<FileInputBinding>& fileInputs = {},
                        const std::vector<FileOutputBinding>& fileOutputs = {}) const;

    /// Rewrites file-backed per-run overrides into explicit host-memory overrides.
    RunSettingsMaterializationResult
    materializeRunSettings(const RunRequest& request) const;

    /// Saves one captured workflow output described by the provided save binding.
    ImageSaveResult
    saveCapturedOutput(const OutputSaveBinding& outputSave, const RunResult& result) const;

    /// Saves all captured workflow outputs described by the provided save bindings.
    SaveOutputsResult
    saveCapturedOutputs(const std::vector<OutputSaveBinding>& outputSaves, const RunResult& result) const;

    /// Materializes file-backed workflow state, then prepares it through \ref Session.
    PrepareWorkflowResult
    prepare(const Session& session,
            const Workflow& workflow,
            const std::vector<FileInputBinding>& fileInputs = {},
            const std::vector<FileOutputBinding>& fileOutputs = {}) const;

    /// Materializes file-backed workflow state, then executes it through \ref Session.
    RunResult
    run(const Session& session,
        const Workflow& workflow,
        const RunRequest& request = {},
        const std::vector<FileInputBinding>& fileInputs = {},
        const std::vector<FileOutputBinding>& fileOutputs = {}) const;

private:
    IoRuntimeOptions m_options;
};

/// Prepared IO-backed workflow that owns deferred output saves.
class PreparedIoWorkflow {
public:
    PreparedIoWorkflow(IoRuntime ioRuntime,
                       std::unique_ptr<PreparedWorkflow> workflow,
                       std::vector<OutputSaveBinding> outputSaves)
        : m_ioRuntime(std::move(ioRuntime))
        , m_workflow(std::move(workflow))
        , m_outputSaves(std::move(outputSaves))
    {
    }

    PreparedIoWorkflow(const PreparedIoWorkflow&) = delete;
    PreparedIoWorkflow& operator=(const PreparedIoWorkflow&) = delete;
    PreparedIoWorkflow(PreparedIoWorkflow&&) = default;
    PreparedIoWorkflow& operator=(PreparedIoWorkflow&&) = default;

    /// Executes the prepared workflow once after materializing file-backed run settings.
    RunResult
    run(const RunRequest& request = {}) const
    {
        if (!m_workflow) {
            RunResult result;
            result.success      = false;
            result.errorMessage = "prepared IO workflow is empty";
            return result;
        }

        const RunSettingsMaterializationResult materializedSettings = m_ioRuntime.materializeRunSettings(request);
        if (!materializedSettings.success) {
            RunResult result;
            result.success      = false;
            result.errorMessage = materializedSettings.errorMessage.empty() ? "run settings materialization failed"
                                                                            : materializedSettings.errorMessage;
            return result;
        }

        RunResult result = m_workflow->run(materializedSettings.settings);
        if (!result.success) {
            return result;
        }

        const SaveOutputsResult saveResult = m_ioRuntime.saveCapturedOutputs(m_outputSaves, result);
        if (!saveResult.success) {
            result.success      = false;
            result.errorMessage = saveResult.errorMessage.empty() ? "captured output save failed"
                                                                  : saveResult.errorMessage;
        }

        return result;
    }

    /// Returns the wrapped prepared core workflow for advanced control.
    const PreparedWorkflow*
    coreWorkflow() const
    {
        return m_workflow.get();
    }

private:
    IoRuntime m_ioRuntime;
    std::unique_ptr<PreparedWorkflow> m_workflow;
    std::vector<OutputSaveBinding> m_outputSaves;
};

/// Loads one file-backed image into \ref HostImageData using a default \ref IoRuntime.
ImageLoadResult
LoadImageFile(const ImageLoadRequest& request);

/// Saves one \ref HostImageData payload to a file-backed image using a default \ref IoRuntime.
ImageSaveResult
SaveImageFile(const ImageSaveRequest& request);

/// Returns image IO capabilities for this RawGL build.
ImageIoCapabilities
GetImageIoCapabilities();

/// Reads metadata from one file-backed image or container using a default \ref IoRuntime.
MetadataReadResult
ReadMetadataFile(const MetadataReadRequest& request);

/// Reads a typed metadata document from one file-backed image or container using a default \ref IoRuntime.
MetadataDocumentReadResult
ReadMetadataDocumentFile(const MetadataDocumentReadRequest& request);

/// Transfers source metadata into an already-written file-backed target image using a default \ref IoRuntime.
ImageMetadataTransferResult
TransferImageMetadataFile(const ImageMetadataTransferRequest& request);

inline PrepareWorkflowResult
IoRuntime::prepare(const Session& session,
                   const Workflow& workflow,
                   const std::vector<FileInputBinding>& fileInputs,
                   const std::vector<FileOutputBinding>& fileOutputs) const
{
    const WorkflowMaterializationResult materializedWorkflow = materializeWorkflow(workflow, fileInputs, fileOutputs);
    if (!materializedWorkflow.success) {
        PrepareWorkflowResult result;
        result.success = false;
        result.errorMessage =
            materializedWorkflow.errorMessage.empty() ? "workflow materialization failed"
                                                      : materializedWorkflow.errorMessage;
        return result;
    }

    PrepareResult preparedResult = session.prepare(materializedWorkflow.workflow);

    PrepareWorkflowResult result;
    result.success      = preparedResult.success;
    result.errorMessage = std::move(preparedResult.errorMessage);
    if (preparedResult.workflow) {
        result.workflow = std::make_unique<PreparedIoWorkflow>(*this,
                                                               std::move(preparedResult.workflow),
                                                               materializedWorkflow.outputSaves);
    }
    return result;
}

inline RunResult
IoRuntime::run(const Session& session,
               const Workflow& workflow,
               const RunRequest& request,
               const std::vector<FileInputBinding>& fileInputs,
               const std::vector<FileOutputBinding>& fileOutputs) const
{
    PrepareWorkflowResult preparedResult = prepare(session, workflow, fileInputs, fileOutputs);
    if (!preparedResult.success || !preparedResult.workflow) {
        RunResult result;
        result.success = false;
        result.errorMessage =
            preparedResult.errorMessage.empty() ? "workflow preparation failed" : preparedResult.errorMessage;
        return result;
    }

    return preparedResult.workflow->run(request);
}

}  // namespace rawgl::io
