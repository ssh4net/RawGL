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

/// Describes one file-backed image load request.
struct ImageLoadRequest {
    /// Source image path.
    std::string path;
    /// Loader attributes such as colorspace hints.
    std::vector<Attribute> attributes;
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

/// Describes one file-backed image save request.
struct ImageSaveRequest {
    /// Destination image path.
    std::string path;
    /// Codec-specific writer hints.
    std::vector<Attribute> attributes;
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

/// One file-backed workflow input owned by `rawgl::io`.
struct FileInputBinding {
    size_t passIndex = 0;
    std::string name;
    std::string path;
    std::vector<Attribute> attributes;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
};

/// One file-backed per-run input override owned by `rawgl::io`.
struct FileInputOverride {
    size_t passIndex = 0;
    std::string name;
    std::string path;
    std::vector<Attribute> attributes;
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
FileTextureInput(size_t passIndex, std::string name, std::string path, std::vector<Attribute> attributes = {})
{
    FileInputBinding result;
    result.passIndex = passIndex;
    result.name = std::move(name);
    result.path = std::move(path);
    result.attributes = std::move(attributes);
    return result;
}

/// Builds one file-backed image per-run override for use with \ref IoRuntime.
inline FileInputOverride
FileTextureOverride(size_t passIndex, std::string name, std::string path, std::vector<Attribute> attributes = {})
{
    FileInputOverride result;
    result.passIndex = passIndex;
    result.name = std::move(name);
    result.path = std::move(path);
    result.attributes = std::move(attributes);
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
           std::vector<Attribute> attributes = {})
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

/// Reads metadata from one file-backed image or container using a default \ref IoRuntime.
MetadataReadResult
ReadMetadataFile(const MetadataReadRequest& request);

/// Reads a typed metadata document from one file-backed image or container using a default \ref IoRuntime.
MetadataDocumentReadResult
ReadMetadataDocumentFile(const MetadataDocumentReadRequest& request);

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
