// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_batch.h"

#include <condition_variable>
#include <deque>
#include <thread>
#include <utility>
#include <vector>

namespace rawgl::batch {
namespace {

template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(const size_t capacity)
        : m_capacity(capacity > 0 ? capacity : 1)
    {
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    bool
    push(T item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_closed && m_items.size() >= m_capacity) {
            m_pushCondition.wait(lock);
        }
        if (m_closed) {
            return false;
        }

        m_items.push_back(std::move(item));
        lock.unlock();
        m_popCondition.notify_one();
        return true;
    }

    bool
    pop(T& outItem)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_closed && m_items.empty()) {
            m_popCondition.wait(lock);
        }
        if (m_items.empty()) {
            return false;
        }

        outItem = std::move(m_items.front());
        m_items.pop_front();
        lock.unlock();
        m_pushCondition.notify_one();
        return true;
    }

    void
    close()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_closed = true;
        lock.unlock();
        m_popCondition.notify_all();
        m_pushCondition.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_popCondition;
    std::condition_variable m_pushCondition;
    std::deque<T> m_items;
    size_t m_capacity = 1;
    bool m_closed = false;
};

struct PrepareWaitState {
    std::mutex mutex;
    std::condition_variable condition;
    bool ready = false;
    BatchPrepareResult result;
};

static InputBinding
clone_batch_input_binding(const InputBinding& input)
{
    InputBinding result = input;
    if (input.hostTexture) {
        result.hostTexture = std::make_shared<HostImageData>(*input.hostTexture);
    }
    return result;
}

static InputOverride
clone_batch_input_override(const InputOverride& inputOverride)
{
    InputOverride result = inputOverride;
    if (inputOverride.hostTexture) {
        result.hostTexture = std::make_shared<HostImageData>(*inputOverride.hostTexture);
    }
    return result;
}

static Workflow
clone_batch_workflow(const Workflow& workflow)
{
    Workflow result = workflow;
    for (Pass& pass : result.passes) {
        for (InputBinding& input : pass.inputs) {
            if (!input.hostTexture) {
                continue;
            }
            input = clone_batch_input_binding(input);
        }
    }
    return result;
}

static BatchSubmitRequest
clone_batch_submit_request(const BatchSubmitRequest& request)
{
    BatchSubmitRequest result = request;
    for (InputOverride& inputOverride : result.settings.overrides) {
        if (!inputOverride.hostTexture) {
            continue;
        }
        inputOverride = clone_batch_input_override(inputOverride);
    }
    return result;
}

static std::vector<io::OutputSaveBinding>
make_batch_output_saves(const std::vector<io::OutputSaveBinding>& preparedOutputSaves,
                        const std::vector<io::FileOutputBinding>& requestFileOutputs)
{
    std::vector<io::OutputSaveBinding> result = preparedOutputSaves;
    result.reserve(result.size() + requestFileOutputs.size());
    for (const io::FileOutputBinding& fileOutput : requestFileOutputs) {
        result.push_back(io::OutputSaveBinding { fileOutput });
    }
    return result;
}

}  // namespace

struct BatchPreparedWorkflow::State {
    std::unique_ptr<PreparedWorkflow> workflow;
    const io::IoRuntime* ioRuntime = nullptr;
    std::vector<io::OutputSaveBinding> outputSaves;
};

struct BatchJobHandle::State {
    mutable std::mutex mutex;
    std::condition_variable condition;
    bool ready = false;
    BatchResult result;
};

struct BatchRunner::State {
    struct PrepareTask {
        Workflow workflow;
        std::vector<io::FileInputBinding> fileInputs;
        std::vector<io::FileOutputBinding> fileOutputs;
        std::shared_ptr<PrepareWaitState> waitState;
    };

    struct ExecuteTask {
        size_t submitIndex = 0;
        BatchSubmitRequest request;
        std::shared_ptr<BatchPreparedWorkflow::State> workflow;
        std::shared_ptr<BatchJobHandle::State> handle;
        std::shared_ptr<BatchCancellationToken::State> cancellation;
    };

    struct SaveTask {
        size_t submitIndex = 0;
        RunResult runResult;
        std::vector<io::OutputSaveBinding> outputSaves;
        const io::IoRuntime* ioRuntime = nullptr;
        std::shared_ptr<BatchJobHandle::State> handle;
        std::shared_ptr<BatchCancellationToken::State> cancellation;
    };

    struct GpuTask {
        enum class Kind {
            prepare,
            execute,
        };

        Kind kind = Kind::prepare;
        PrepareTask prepareTask;
        ExecuteTask executeTask;
    };

    explicit State(Session& runnerSession, const io::IoRuntime* runnerIoRuntime, const BatchRunnerOptions& runnerOptions)
        : session(&runnerSession)
        , ioRuntime(runnerIoRuntime)
        , options(runnerOptions)
        , gpuQueue(max_gpu_queue_capacity(runnerOptions))
        , saveQueue(runnerOptions.saveQueueCapacity)
    {
        if (options.maxInFlightJobs == 0) {
            options.maxInFlightJobs = 1;
        }
    }

    static size_t
    max_gpu_queue_capacity(const BatchRunnerOptions& options)
    {
        return options.prepareQueueCapacity > options.executeQueueCapacity
            ? options.prepareQueueCapacity
            : options.executeQueueCapacity;
    }

    Session* session = nullptr;
    const io::IoRuntime* ioRuntime = nullptr;
    BatchRunnerOptions options;
    mutable std::mutex progressMutex;
    std::condition_variable progressCondition;
    BatchProgress progress;
    size_t nextSubmitIndex = 0;
    bool stopping = false;
    BoundedQueue<GpuTask> gpuQueue;
    BoundedQueue<SaveTask> saveQueue;
    std::thread gpuWorker;
    std::vector<std::thread> saveWorkers;

    void
    start_workers()
    {
        session->releaseExecutionContext();
        gpuWorker = std::thread(&State::gpu_worker_main, this);

        if (!ioRuntime || options.saveWorkerCount == 0) {
            return;
        }

        saveWorkers.reserve(options.saveWorkerCount);
        for (uint32_t workerIndex = 0; workerIndex < options.saveWorkerCount; ++workerIndex) {
            saveWorkers.emplace_back(&State::save_worker_main, this);
        }
    }

    void
    stop_workers()
    {
        {
            std::lock_guard<std::mutex> lock(progressMutex);
            stopping = true;
        }

        gpuQueue.close();
        saveQueue.close();
        progressCondition.notify_all();

        if (gpuWorker.joinable()) {
            gpuWorker.join();
        }
        for (size_t workerIndex = 0; workerIndex < saveWorkers.size(); ++workerIndex) {
            if (saveWorkers[workerIndex].joinable()) {
                saveWorkers[workerIndex].join();
            }
        }

        session->makeExecutionContextCurrent();
    }

    bool
    cancellation_requested(const std::shared_ptr<BatchCancellationToken::State>& cancellation) const
    {
        if (!cancellation) {
            return false;
        }

        std::lock_guard<std::mutex> lock(cancellation->mutex);
        return cancellation->requested;
    }

    BatchJobHandle
    make_ready_handle(const size_t submitIndex, const std::string& message, const bool cancelled)
    {
        std::shared_ptr<BatchJobHandle::State> handleState = std::make_shared<BatchJobHandle::State>();
        {
            std::lock_guard<std::mutex> lock(handleState->mutex);
            handleState->ready = true;
            handleState->result.submitIndex = submitIndex;
            handleState->result.cancelled = cancelled;
            handleState->result.runResult.success = false;
            handleState->result.runResult.errorMessage = message;
        }
        handleState->condition.notify_all();
        return BatchJobHandle(handleState);
    }

    bool
    should_stop() const
    {
        std::lock_guard<std::mutex> lock(progressMutex);
        return stopping;
    }

    void
    finish_cancelled(const std::shared_ptr<BatchJobHandle::State>& handle,
                     const size_t submitIndex,
                     const std::string& message)
    {
        {
            std::lock_guard<std::mutex> lock(handle->mutex);
            if (handle->ready) {
                return;
            }
            handle->ready = true;
            handle->result.submitIndex = submitIndex;
            handle->result.cancelled = true;
            handle->result.runResult.success = false;
            handle->result.runResult.errorMessage = message;
        }
        handle->condition.notify_all();

        std::lock_guard<std::mutex> lock(progressMutex);
        ++progress.cancelledJobs;
        if (progress.inFlightJobs > 0) {
            --progress.inFlightJobs;
        }
        progressCondition.notify_all();
    }

    void
    finish_result(const std::shared_ptr<BatchJobHandle::State>& handle, BatchResult result)
    {
        const bool cancelled = result.cancelled;
        const bool success = result.runResult.success;

        {
            std::lock_guard<std::mutex> lock(handle->mutex);
            if (handle->ready) {
                return;
            }
            handle->ready = true;
            handle->result = std::move(result);
        }
        handle->condition.notify_all();

        std::lock_guard<std::mutex> lock(progressMutex);
        if (cancelled) {
            ++progress.cancelledJobs;
        } else if (success) {
            ++progress.completedJobs;
        } else {
            ++progress.failedJobs;
        }
        if (progress.inFlightJobs > 0) {
            --progress.inFlightJobs;
        }
        progressCondition.notify_all();
    }

    void
    complete_prepare(std::shared_ptr<PrepareWaitState> waitState, BatchPrepareResult result)
    {
        {
            std::lock_guard<std::mutex> lock(waitState->mutex);
            waitState->ready = true;
            waitState->result = std::move(result);
        }
        waitState->condition.notify_all();
    }

    BatchPrepareResult
    prepare_workflow(const Workflow& workflow,
                     const std::vector<io::FileInputBinding>& fileInputs,
                     const std::vector<io::FileOutputBinding>& fileOutputs)
    {
        if (ioRuntime) {
            const io::WorkflowMaterializationResult materialized =
                ioRuntime->materializeWorkflow(workflow, fileInputs, fileOutputs);
            if (!materialized.success) {
                BatchPrepareResult result;
                result.success = false;
                result.errorMessage = materialized.errorMessage.empty() ? "batch workflow materialization failed"
                                                                        : materialized.errorMessage;
                return result;
            }

            PrepareResult prepareResult = session->prepare(materialized.workflow);

            BatchPrepareResult result;
            result.success = prepareResult.success;
            result.errorMessage = std::move(prepareResult.errorMessage);
            if (prepareResult.workflow) {
                std::shared_ptr<BatchPreparedWorkflow::State> workflowState = std::make_shared<BatchPreparedWorkflow::State>();
                workflowState->workflow = std::move(prepareResult.workflow);
                workflowState->ioRuntime = ioRuntime;
                workflowState->outputSaves = materialized.outputSaves;
                result.workflow = std::unique_ptr<BatchPreparedWorkflow>(new BatchPreparedWorkflow(std::move(workflowState)));
            }
            return result;
        }

        PrepareResult prepareResult = session->prepare(workflow);

        BatchPrepareResult result;
        result.success = prepareResult.success;
        result.errorMessage = std::move(prepareResult.errorMessage);
        if (prepareResult.workflow) {
            std::shared_ptr<BatchPreparedWorkflow::State> workflowState = std::make_shared<BatchPreparedWorkflow::State>();
            workflowState->workflow = std::move(prepareResult.workflow);
            result.workflow = std::unique_ptr<BatchPreparedWorkflow>(new BatchPreparedWorkflow(std::move(workflowState)));
        }
        return result;
    }

    void
    gpu_worker_main()
    {
        session->makeExecutionContextCurrent();

        GpuTask task;
        while (gpuQueue.pop(task)) {
            if (task.kind == GpuTask::Kind::prepare) {
                if (should_stop()) {
                    BatchPrepareResult result;
                    result.success = false;
                    result.errorMessage = "batch runner stopped";
                    complete_prepare(task.prepareTask.waitState, std::move(result));
                    task = GpuTask {};
                    continue;
                }
                complete_prepare(task.prepareTask.waitState,
                                 prepare_workflow(task.prepareTask.workflow,
                                                  task.prepareTask.fileInputs,
                                                  task.prepareTask.fileOutputs));
                task = GpuTask {};
                continue;
            }

            const ExecuteTask& executeTask = task.executeTask;
            if (should_stop()) {
                finish_cancelled(executeTask.handle, executeTask.submitIndex, "batch runner stopped");
                task = GpuTask {};
                continue;
            }
            if (cancellation_requested(executeTask.cancellation)) {
                finish_cancelled(executeTask.handle, executeTask.submitIndex, "batch job cancelled before execution");
                task = GpuTask {};
                continue;
            }

            RunSettings effectiveSettings = executeTask.request.settings;
            if (executeTask.workflow->ioRuntime) {
                io::RunSettingsMaterializationResult materialized =
                    executeTask.workflow->ioRuntime->materializeRunSettings(
                        io::RunRequest { executeTask.request.settings, executeTask.request.fileInputs });
                if (!materialized.success) {
                    BatchResult result;
                    result.submitIndex = executeTask.submitIndex;
                    result.runResult.success = false;
                    result.runResult.errorMessage = materialized.errorMessage.empty()
                        ? "batch run settings materialization failed"
                        : materialized.errorMessage;
                    finish_result(executeTask.handle, std::move(result));
                    task = GpuTask {};
                    continue;
                }
                effectiveSettings = std::move(materialized.settings);
            }

            if (cancellation_requested(executeTask.cancellation)) {
                finish_cancelled(executeTask.handle, executeTask.submitIndex, "batch job cancelled before execution");
                task = GpuTask {};
                continue;
            }

            BatchResult result;
            result.submitIndex = executeTask.submitIndex;
            result.runResult = executeTask.workflow->workflow->run(effectiveSettings);

            if (!result.runResult.success) {
                finish_result(executeTask.handle, std::move(result));
                task = GpuTask {};
                continue;
            }

            if (!executeTask.workflow->ioRuntime) {
                if (!executeTask.request.fileOutputs.empty()) {
                    result.runResult.success = false;
                    result.runResult.errorMessage = "batch per-run file outputs require an IO runtime";
                }
                finish_result(executeTask.handle, std::move(result));
                task = GpuTask {};
                continue;
            }

            std::vector<io::OutputSaveBinding> combinedOutputSaves;
            const std::vector<io::OutputSaveBinding>* outputSaves = &executeTask.workflow->outputSaves;
            if (!executeTask.request.fileOutputs.empty()) {
                combinedOutputSaves =
                    make_batch_output_saves(executeTask.workflow->outputSaves, executeTask.request.fileOutputs);
                outputSaves = &combinedOutputSaves;
            }
            if (outputSaves->empty()) {
                finish_result(executeTask.handle, std::move(result));
                task = GpuTask {};
                continue;
            }

            if (saveWorkers.empty()) {
                const io::SaveOutputsResult saveResult =
                    executeTask.workflow->ioRuntime->saveCapturedOutputs(*outputSaves, result.runResult);
                if (!saveResult.success) {
                    result.runResult.success = false;
                    result.runResult.errorMessage = saveResult.errorMessage.empty()
                        ? "batch output save failed"
                        : saveResult.errorMessage;
                }
                finish_result(executeTask.handle, std::move(result));
                task = GpuTask {};
                continue;
            }

            SaveTask saveTask;
            saveTask.submitIndex = executeTask.submitIndex;
            saveTask.runResult = std::move(result.runResult);
            if (!executeTask.request.fileOutputs.empty()) {
                saveTask.outputSaves = std::move(combinedOutputSaves);
            } else {
                saveTask.outputSaves = executeTask.workflow->outputSaves;
            }
            saveTask.ioRuntime = executeTask.workflow->ioRuntime;
            saveTask.handle = executeTask.handle;
            saveTask.cancellation = executeTask.cancellation;

            if (!saveQueue.push(std::move(saveTask))) {
                BatchResult failedResult;
                failedResult.submitIndex = executeTask.submitIndex;
                failedResult.runResult.success = false;
                failedResult.runResult.errorMessage = "batch save queue is closed";
                finish_result(executeTask.handle, std::move(failedResult));
            }
            task = GpuTask {};
        }

        session->releaseExecutionContext();
    }

    void
    save_worker_main()
    {
        SaveTask task;
        while (saveQueue.pop(task)) {
            if (should_stop()) {
                finish_cancelled(task.handle, task.submitIndex, "batch runner stopped");
                task = SaveTask {};
                continue;
            }
            if (cancellation_requested(task.cancellation)) {
                finish_cancelled(task.handle, task.submitIndex, "batch job cancelled before output save");
                task = SaveTask {};
                continue;
            }

            BatchResult result;
            result.submitIndex = task.submitIndex;
            result.runResult = std::move(task.runResult);

            const io::SaveOutputsResult saveResult = task.ioRuntime->saveCapturedOutputs(task.outputSaves,
                                                                                         result.runResult);
            if (!saveResult.success) {
                result.runResult.success = false;
                result.runResult.errorMessage =
                    saveResult.errorMessage.empty() ? "batch output save failed" : saveResult.errorMessage;
            }

            finish_result(task.handle, std::move(result));
            task = SaveTask {};
        }
    }
};

BatchCancellationToken::BatchCancellationToken()
    : m_state(std::make_shared<State>())
{
}

BatchCancellationToken::~BatchCancellationToken() = default;

void
BatchCancellationToken::cancel()
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    m_state->requested = true;
}

bool
BatchCancellationToken::isCancellationRequested() const
{
    std::lock_guard<std::mutex> lock(m_state->mutex);
    return m_state->requested;
}

BatchPreparedWorkflow::BatchPreparedWorkflow(std::shared_ptr<State> state)
    : m_state(std::move(state))
{
}

BatchPreparedWorkflow::~BatchPreparedWorkflow() = default;

BatchJobHandle::BatchJobHandle() = default;

BatchJobHandle::BatchJobHandle(std::shared_ptr<State> state)
    : m_state(std::move(state))
{
}

BatchJobHandle::~BatchJobHandle() = default;

BatchResult
BatchJobHandle::wait() const
{
    BatchResult result;
    if (!m_state) {
        result.runResult.success = false;
        result.runResult.errorMessage = "batch job handle is empty";
        return result;
    }

    std::unique_lock<std::mutex> lock(m_state->mutex);
    while (!m_state->ready) {
        m_state->condition.wait(lock);
    }
    return m_state->result;
}

BatchRunner::BatchRunner(Session& session, const BatchRunnerOptions& options)
    : m_state(std::make_unique<State>(session, nullptr, options))
{
    m_state->start_workers();
}

BatchRunner::BatchRunner(Session& session, const io::IoRuntime& ioRuntime, const BatchRunnerOptions& options)
    : m_state(std::make_unique<State>(session, &ioRuntime, options))
{
    m_state->start_workers();
}

BatchRunner::~BatchRunner()
{
    close();
}

BatchPrepareResult
BatchRunner::prepare(const Workflow& workflow,
                     const std::vector<io::FileInputBinding>& fileInputs,
                     const std::vector<io::FileOutputBinding>& fileOutputs) const
{
    if (!m_state) {
        BatchPrepareResult result;
        result.success = false;
        result.errorMessage = "batch runner is empty";
        return result;
    }

    std::shared_ptr<PrepareWaitState> waitState = std::make_shared<PrepareWaitState>();
    State::GpuTask task;
    task.kind = State::GpuTask::Kind::prepare;
    task.prepareTask.workflow = clone_batch_workflow(workflow);
    task.prepareTask.fileInputs = fileInputs;
    task.prepareTask.fileOutputs = fileOutputs;
    task.prepareTask.waitState = waitState;

    if (!m_state->gpuQueue.push(std::move(task))) {
        BatchPrepareResult result;
        result.success = false;
        result.errorMessage = "batch prepare queue is closed";
        return result;
    }

    std::unique_lock<std::mutex> lock(waitState->mutex);
    while (!waitState->ready) {
        waitState->condition.wait(lock);
    }
    return std::move(waitState->result);
}

BatchJobHandle
BatchRunner::submit(const BatchPreparedWorkflow& workflow,
                    const BatchSubmitRequest& request,
                    const BatchCancellationToken* cancellation)
{
    if (!m_state) {
        return BatchJobHandle();
    }
    if (!workflow.m_state || !workflow.m_state->workflow) {
        return m_state->make_ready_handle(0, "batch prepared workflow is empty", false);
    }

    std::unique_lock<std::mutex> progressLock(m_state->progressMutex);
    while (!m_state->stopping && m_state->progress.inFlightJobs >= m_state->options.maxInFlightJobs) {
        m_state->progressCondition.wait(progressLock);
    }

    const size_t submitIndex = m_state->nextSubmitIndex;
    ++m_state->nextSubmitIndex;

    if (m_state->stopping) {
        progressLock.unlock();
        return m_state->make_ready_handle(submitIndex, "batch runner stopped", true);
    }

    if (cancellation && m_state->cancellation_requested(cancellation->m_state)) {
        ++m_state->progress.submittedJobs;
        ++m_state->progress.cancelledJobs;
        progressLock.unlock();
        m_state->progressCondition.notify_all();
        return m_state->make_ready_handle(submitIndex, "batch job cancelled before submission", true);
    }

    std::shared_ptr<BatchJobHandle::State> handleState = std::make_shared<BatchJobHandle::State>();
    ++m_state->progress.submittedJobs;
    ++m_state->progress.inFlightJobs;
    progressLock.unlock();

    State::GpuTask task;
    task.kind = State::GpuTask::Kind::execute;
    task.executeTask.submitIndex = submitIndex;
    task.executeTask.request = clone_batch_submit_request(request);
    task.executeTask.workflow = workflow.m_state;
    task.executeTask.handle = handleState;
    if (cancellation) {
        task.executeTask.cancellation = cancellation->m_state;
    }

    if (!m_state->gpuQueue.push(std::move(task))) {
        BatchResult result;
        result.submitIndex = submitIndex;
        result.runResult.success = false;
        result.runResult.errorMessage = "batch execute queue is closed";
        {
            std::lock_guard<std::mutex> lock(handleState->mutex);
            handleState->ready = true;
            handleState->result = result;
        }
        handleState->condition.notify_all();

        std::lock_guard<std::mutex> lock(m_state->progressMutex);
        ++m_state->progress.failedJobs;
        if (m_state->progress.inFlightJobs > 0) {
            --m_state->progress.inFlightJobs;
        }
        m_state->progressCondition.notify_all();
    }

    return BatchJobHandle(handleState);
}

BatchProgress
BatchRunner::progress() const
{
    if (!m_state) {
        return BatchProgress {};
    }

    std::lock_guard<std::mutex> lock(m_state->progressMutex);
    return m_state->progress;
}

void
BatchRunner::close()
{
    if (!m_state) {
        return;
    }

    m_state->stop_workers();
    m_state.reset();
}

}  // namespace rawgl::batch
