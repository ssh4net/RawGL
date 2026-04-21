// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <rawgl/rawgl.h>

namespace rawgl::io {

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
    /// Writer attributes such as OpenImageIO metadata.
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

/// One deferred file-backed workflow output.
struct OutputSaveBinding {
    /// Workflow pass that produced this output.
    size_t passIndex = 0;
    /// Original workflow output declaration.
    OutputBinding output;
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

    /// Rewrites file-backed workflow inputs and outputs into explicit host-memory workflow state.
    WorkflowMaterializationResult
    materializeWorkflow(const Workflow& workflow) const;

    /// Rewrites file-backed per-run overrides into explicit host-memory overrides.
    RunSettingsMaterializationResult
    materializeRunSettings(const RunSettings& settings) const;

    /// Saves one captured workflow output described by \ref outputSave.
    ImageSaveResult
    saveCapturedOutput(const OutputSaveBinding& outputSave, const RunResult& result) const;

    /// Saves all captured workflow outputs described by \ref outputSaves.
    SaveOutputsResult
    saveCapturedOutputs(const std::vector<OutputSaveBinding>& outputSaves, const RunResult& result) const;

    /// Materializes file-backed workflow state, then prepares it through \ref Session.
    PrepareWorkflowResult
    prepare(const Session& session, const Workflow& workflow) const;

    /// Materializes file-backed workflow state, then executes it through \ref Session.
    RunResult
    run(const Session& session, const Workflow& workflow, const RunSettings& settings = {}) const;

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
    run(const RunSettings& settings = {}) const
    {
        if (!m_workflow) {
            RunResult result;
            result.success      = false;
            result.errorMessage = "prepared IO workflow is empty";
            return result;
        }

        const RunSettingsMaterializationResult materializedSettings = m_ioRuntime.materializeRunSettings(settings);
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

inline PrepareWorkflowResult
IoRuntime::prepare(const Session& session, const Workflow& workflow) const
{
    const WorkflowMaterializationResult materializedWorkflow = materializeWorkflow(workflow);
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
IoRuntime::run(const Session& session, const Workflow& workflow, const RunSettings& settings) const
{
    PrepareWorkflowResult preparedResult = prepare(session, workflow);
    if (!preparedResult.success || !preparedResult.workflow) {
        RunResult result;
        result.success = false;
        result.errorMessage =
            preparedResult.errorMessage.empty() ? "workflow preparation failed" : preparedResult.errorMessage;
        return result;
    }

    return preparedResult.workflow->run(settings);
}

}  // namespace rawgl::io
