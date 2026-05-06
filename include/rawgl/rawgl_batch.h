// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include <rawgl/rawgl_io.h>

namespace rawgl::batch {

/// Controls queue sizing, worker intent, and budget hints for future batch execution.
///
/// The current implementation uses a bounded GPU command queue, one GPU worker,
/// and an optional save-worker pool while keeping `rawgl_core` itself synchronous.
struct BatchRunnerOptions {
    size_t maxInFlightJobs = 8;
    size_t prepareQueueCapacity = 8;
    size_t executeQueueCapacity = 4;
    size_t saveQueueCapacity = 8;
    uint32_t prepareWorkerCount = 2;
    uint32_t saveWorkerCount = 2;
    uint32_t gpuWorkerCount = 1;
    uint64_t hostMemoryBudgetBytes = 0;
    uint64_t gpuMemoryBudgetBytes = 0;
    bool preserveSubmitOrder = true;
};

/// Snapshot of batch progress counters.
struct BatchProgress {
    size_t submittedJobs = 0;
    size_t completedJobs = 0;
    size_t failedJobs = 0;
    size_t cancelledJobs = 0;
    size_t inFlightJobs = 0;
};

/// Per-submit run payload for a prepared batch workflow.
struct BatchSubmitRequest {
    /// In-memory per-run settings and overrides.
    RunSettings settings;
    /// File-backed per-run input overrides. Requires a batch runner constructed with `IoRuntime`.
    std::vector<io::FileInputOverride> fileInputs;
    /// File-backed per-run output targets. The referenced workflow outputs must be captured.
    std::vector<io::FileOutputBinding> fileOutputs;
};

/// Result of one submitted batch job.
struct BatchResult {
    size_t submitIndex = 0;
    bool cancelled = false;
    RunResult runResult;
};

/// Shared cancellation token for one or more submitted jobs.
class BatchCancellationToken {
public:
    BatchCancellationToken();
    ~BatchCancellationToken();

    BatchCancellationToken(const BatchCancellationToken&) = default;
    BatchCancellationToken& operator=(const BatchCancellationToken&) = default;
    BatchCancellationToken(BatchCancellationToken&&) noexcept = default;
    BatchCancellationToken& operator=(BatchCancellationToken&&) noexcept = default;

    void cancel();
    bool isCancellationRequested() const;

private:
    friend class BatchRunner;

    struct State {
        mutable std::mutex mutex;
        bool requested = false;
    };

    std::shared_ptr<State> m_state;
};

/// Prepared workflow owned by the batch layer.
///
/// This currently wraps either a prepared core workflow or a prepared
/// IO-materialized workflow. It does not own worker threads.
class BatchPreparedWorkflow {
public:
    BatchPreparedWorkflow(const BatchPreparedWorkflow&) = delete;
    BatchPreparedWorkflow& operator=(const BatchPreparedWorkflow&) = delete;
    BatchPreparedWorkflow(BatchPreparedWorkflow&&) noexcept = default;
    BatchPreparedWorkflow& operator=(BatchPreparedWorkflow&&) noexcept = default;
    ~BatchPreparedWorkflow();

private:
    friend class BatchRunner;
    struct State;

    explicit BatchPreparedWorkflow(std::shared_ptr<State> state);

    std::shared_ptr<State> m_state;
};

/// Result of preparing one batch workflow.
struct BatchPrepareResult {
    bool success = false;
    std::string errorMessage;
    std::unique_ptr<BatchPreparedWorkflow> workflow;
};

/// Handle for one submitted batch job.
///
/// `wait()` blocks until the queued job reaches a terminal state.
class BatchJobHandle {
public:
    BatchJobHandle();
    ~BatchJobHandle();

    BatchJobHandle(const BatchJobHandle&) = delete;
    BatchJobHandle& operator=(const BatchJobHandle&) = delete;
    BatchJobHandle(BatchJobHandle&&) noexcept = default;
    BatchJobHandle& operator=(BatchJobHandle&&) noexcept = default;

    BatchResult wait() const;

private:
    friend class BatchRunner;
    struct State;

    explicit BatchJobHandle(std::shared_ptr<State> state);

    std::shared_ptr<State> m_state;
};

/// Batch orchestration façade on top of `Session` and optional `IoRuntime`.
///
/// The current implementation uses one GPU worker for `Session` preparation and
/// execution, plus optional save workers for deferred file output writes.
class BatchRunner {
public:
    explicit BatchRunner(Session& session, const BatchRunnerOptions& options = {});
    BatchRunner(Session& session, const io::IoRuntime& ioRuntime, const BatchRunnerOptions& options = {});
    ~BatchRunner();

    BatchRunner(const BatchRunner&) = delete;
    BatchRunner& operator=(const BatchRunner&) = delete;
    BatchRunner(BatchRunner&&) = delete;
    BatchRunner& operator=(BatchRunner&&) = delete;

    BatchPrepareResult prepare(const Workflow& workflow,
                               const std::vector<io::FileInputBinding>& fileInputs = {},
                               const std::vector<io::FileOutputBinding>& fileOutputs = {}) const;

    BatchJobHandle
    submit(const BatchPreparedWorkflow& workflow,
           const BatchSubmitRequest& request = {},
           const BatchCancellationToken* cancellation = nullptr);

    BatchProgress progress() const;
    void close();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

}  // namespace rawgl::batch
