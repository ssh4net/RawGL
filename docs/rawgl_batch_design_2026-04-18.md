# RawGL Batch Design

Date: 2026-04-18

Purpose: define the first public `rawgl_batch` layer on top of:

- `rawgl_core`
- optional `rawgl_io`

This note is about orchestration, backpressure, and public API shape. It is not
an implementation patch.

Related notes:

- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)
- [rawgl_type_support_and_python_binding_plan_2026-04-15.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_type_support_and_python_binding_plan_2026-04-15.md)

Reference implementations reviewed before this draft:

- [do_process.cpp](/mnt/w/VisualStudio/UnRAWer/UnRAWer/src/do_process.cpp)
- [threadpool.h](/mnt/w/VisualStudio/UnRAWer/UnRAWer/src/threadpool.h)
- [batch_plan.md](/mnt/w/VisualStudio/Photonia/batch_plan.md)
- [pm_batch_queue.h](/mnt/w/VisualStudio/Photonia/photonia_core/src/include/pm_batch_queue.h)
- [pm_batch_runtime.cpp](/mnt/w/VisualStudio/Photonia/photonia_core/src/framework/batch/pm_batch_runtime.cpp)

## 1. Problem statement

`rawgl_core` already supports:

- `Session`
- `PreparedWorkflow`
- synchronous `run(...)`
- host-memory upload and capture

`rawgl_io` now supports:

- file-backed image load/save
- file-backed workflow materialization
- `IoRuntime::prepare(...)`
- `IoRuntime::run(...)`

What is still missing is the advanced throughput layer:

- submit many runs
- overlap file load, upload, execute, readback, save
- bound memory growth
- expose cancellation and progress
- preserve deterministic result ordering

That belongs in `rawgl_batch`, not in hidden core-side threads.

## 2. Design influences

### 2.1 UnRAWer influence

Relevant patterns from [do_process.cpp](/mnt/w/VisualStudio/UnRAWer/UnRAWer/src/do_process.cpp):

- explicit stage-specific thread pools
- separate progress worker
- each stage enqueues the next stage
- stage widths are independently tunable
- the implementation favors directness over formal queue contracts

What RawGL should keep:

- explicit staged orchestration instead of one giant generic worker pool
- separate controls for IO-heavy vs GPU-heavy stages
- progress aggregation as a first-class concern

What RawGL should not copy directly:

- ad hoc global pool map ownership
- unbounded cross-stage state growth
- loose queue contracts

### 2.2 Photonia influence

Relevant patterns from [batch_plan.md](/mnt/w/VisualStudio/Photonia/batch_plan.md),
[pm_batch_queue.h](/mnt/w/VisualStudio/Photonia/photonia_core/src/include/pm_batch_queue.h),
and [pm_batch_runtime.cpp](/mnt/w/VisualStudio/Photonia/photonia_core/src/framework/batch/pm_batch_runtime.cpp):

- fixed-capacity queues between stages
- blocking push/pop with close semantics
- explicit cancellation
- explicit per-stage task structs
- budget-token pools for VRAM and host memory
- deterministic planner/runtime separation

What RawGL should keep:

- bounded queues with backpressure
- explicit close and cancellation propagation
- explicit stage task records
- explicit memory budgeting

## 3. Module boundary

### 3.1 `rawgl_core`

Stays responsible for:

- workflow preparation
- synchronous execution
- GPU resource ownership
- host-memory upload and capture

It should not own:

- worker groups
- queue topology
- batch progress aggregation
- retry policy
- multi-job scheduling

### 3.2 `rawgl_io`

Stays responsible for:

- file decode
- file encode
- workflow/run-settings materialization for file-backed resources

It may be used by `rawgl_batch`, but batch orchestration must also work without
it for pure host-memory workflows.

### 3.3 `rawgl_batch`

Owns:

- stage queues
- worker groups
- batch progress and cancellation
- in-flight job budgeting
- overlap of IO, execution, and save

Dependency direction:

- `rawgl_batch` depends on `rawgl_core`
- `rawgl_batch` may optionally hold an `IoRuntime`

## 4. First public API shape

The first public batch surface should stay intentionally small.

### 4.1 Options

Recommended first type:

- `struct BatchRunnerOptions`

Initial fields:

- `size_t maxInFlightJobs = 8`
- `size_t prepareQueueCapacity = 8`
- `size_t executeQueueCapacity = 4`
- `size_t saveQueueCapacity = 8`
- `uint32_t prepareWorkerCount = 2`
- `uint32_t saveWorkerCount = 2`
- `uint32_t gpuWorkerCount = 1`
- `uint64_t hostMemoryBudgetBytes = 0`
- `uint64_t gpuMemoryBudgetBytes = 0`
- `bool preserveSubmitOrder = true`

Rules:

- default `gpuWorkerCount` should be `1`
- values above `1` must be capability- and backend-gated later
- zero budget means "no explicit budget limit"

### 4.2 Core runtime objects

Recommended first public types:

- `class BatchRunner`
- `class BatchPreparedWorkflow`
- `class BatchJobHandle`
- `struct BatchSubmitRequest`
- `struct BatchResult`
- `struct BatchProgress`
- `class BatchCancellationToken`

### 4.3 Submit model

Recommended first model:

- prepare once
- submit many run settings
- collect results in submit order by default

Illustrative shape:

```cpp
rawgl::Session session;
rawgl::io::IoRuntime io_runtime;
rawgl::batch::BatchRunner runner(session, io_runtime, options);

auto prepared = runner.prepare(workflow);
auto handle_a = runner.submit(prepared, run_settings_a);
auto handle_b = runner.submit(prepared, run_settings_b);

BatchResult result_a = handle_a.wait();
BatchResult result_b = handle_b.wait();
```

For in-memory-only workflows:

```cpp
rawgl::batch::BatchRunner runner(session, options);
```

## 5. Pipeline model

The first runtime pipeline should stay simple and explicit:

1. materialize
   - optional `IoRuntime` conversion from file-backed workflow data to host-memory data
   - per-run override materialization
2. execute
   - `PreparedWorkflow::run(...)`
3. save
   - optional deferred output saves through `IoRuntime`

This is intentionally narrower than Photonia's current `Read / Decode / Normals / Post / Write`
pipeline because RawGL already treats shader execution as the central stage.

### 5.1 Stage task records

Recommended first internal task records:

- `PreparedJobTask`
- `MaterializedRunTask`
- `ExecutedRunTask`
- `SaveTask`

The goal is to keep state handoff explicit and serializable in logs/diagnostics.

### 5.2 Queue rules

RawGL should copy Photonia's bounded-queue behavior, not UnRAWer's looser pool-only model:

- each stage queue has fixed capacity
- producer blocks when downstream is full
- consumer blocks when upstream is empty
- queue close wakes blocked workers

Why:

- memory growth stays bounded
- backpressure is automatic
- progress/cancellation become easier to reason about

## 6. Session and GPU worker model

OpenGL is still the active backend. That means:

- one `Session` should normally be owned by one GPU execution worker
- `gpuWorkerCount > 1` should not be enabled by default
- later multi-GPU or multi-context concurrency must be explicit

So the first implementation target should be:

- many CPU-side materialize/save workers
- one GPU execution worker

That still gives useful overlap:

- decode next inputs while current job runs
- save finished outputs while next job runs

without hiding unsafe context behavior in the core.

## 7. Memory and backpressure policy

The first batch API should support two independent budgets:

- host-memory budget
- GPU-side transient budget

This is influenced directly by Photonia's token-pool approach in
[pm_batch_runtime.cpp](/mnt/w/VisualStudio/Photonia/photonia_core/src/framework/batch/pm_batch_runtime.cpp).

Recommended rules:

- host budget gates materialized inputs and captured outputs kept in-flight
- GPU budget gates jobs waiting to execute or holding captured outputs before save
- a job that would exceed budget blocks before entering the next stage

The first implementation can keep this simple:

- byte estimates are approximate
- failure to estimate should fall back to queue-count limits

## 8. Progress and cancellation

### 8.1 Progress

Progress should be explicit and queryable, not inferred from logs.

Recommended first fields:

- `submittedJobs`
- `materializedJobs`
- `executedJobs`
- `savedJobs`
- `failedJobs`
- `cancelledJobs`
- `inFlightJobs`

### 8.2 Cancellation

Cancellation should propagate through all queues and workers:

- no new submissions accepted after cancellation
- blocking workers wake and exit
- queued but unstarted jobs become cancelled results
- the first hard failure may optionally trigger whole-run cancellation

Rule:

- first implementation should default to `cancel_on_first_error = false`
- later policy options can be added explicitly

## 9. Determinism rules

The first `rawgl_batch` surface should preserve submit order by default.

Why:

- it matches frontend expectations
- it simplifies testing
- it makes Python iteration predictable

Implementation rule:

- stage execution may complete out of order internally
- result publication should reorder by submit index when `preserveSubmitOrder = true`

## 10. Relationship to lifetime hints

`rawgl_batch` should consume, not replace, the existing resource lifetime hints.

Expected use:

- `per_execution`
  - ordinary one-shot job data
- `per_workflow`
  - stable across repeated runs of the same prepared workflow
- `per_batch`
  - stable across many jobs submitted to one `BatchRunner`
- `shared_session`
  - candidate for reuse beyond one runner lifetime

The batch layer should use these hints for scheduling and staging decisions,
but not expose core cache internals.

## 11. Python-facing implications

The default Python API should stay synchronous:

- `rawgl.image(...)`
- `rawgl.prepare_image(...)`
- `PreparedJob.run(...)`

The advanced Python batch path should be separate:

```python
runner = rawgl.batch.BatchRunner(session, io_runtime=io_runtime)
prepared = runner.prepare_image(fragment_shader=FRAGMENT, output="out.png")
handle = runner.submit(prepared, inputs={"u_src0": image_array})
result = handle.result()
```

That keeps notebooks simple while still giving a serious throughput path.

## 12. First implementation phase

The first implementation should stay smaller than the full design.

### Phase 1 target

- public note and API skeleton only
- no behavior change in current synchronous paths

### Phase 2 target

- `BatchRunnerOptions`
- one bounded submission queue
- one GPU worker
- optional save worker pool
- simple result handles
- in-memory workflows first

### Phase 3 target

- integrate `IoRuntime` materialization and save stages
- add host-memory budget control
- add progress snapshots

### Phase 4 target

- add explicit GPU memory budget control
- add policy knobs for cancel/retry
- add Python bindings

## 13. Acceptance criteria

The first meaningful `rawgl_batch` landing should satisfy:

- `rawgl_core` remains synchronous and unchanged for one-shot use
- `rawgl_batch` can prepare once and submit many runs
- bounded queues prevent unbounded memory growth
- cancellation is explicit and testable
- default execution uses one GPU worker
- `rawgl_batch` works both:
  - with `rawgl_core` only for host-memory workflows
  - with `rawgl_core + rawgl_io` for file-backed workflows
- result ordering is deterministic by default
- Python can later expose it without inventing a separate orchestration model
