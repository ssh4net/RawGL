..
  SPDX-License-Identifier: Apache-2.0

Batch
=====

``rawgl::batch::BatchRunner`` is the public orchestration layer on top of
``Session`` and optional ``IoRuntime``.

It exists because repeated multi-entry processing has different concerns than
single-workflow execution:

- submission queues
- progress
- cancellation
- save overlap
- bounded in-flight work

What BatchRunner is
-------------------

``BatchRunner`` prepares and submits workflows through a scheduling layer.

Today it uses:

- one graphics processing unit (GPU) worker for workflow preparation and execution
- optional save workers for deferred file output writes
- bounded queue behavior through the batch options

This is intentionally separate from ``Session``. ``Session`` remains the
long-lived execution owner; ``BatchRunner`` is the orchestrator.

Typical C++ shape
-----------------

.. code-block:: cpp

   #include <rawgl/rawgl.h>
   #include <rawgl/rawgl_batch.h>

   rawgl::Session session;
   rawgl::batch::BatchRunner runner(session);

   rawgl::Workflow workflow;
   // Fill workflow...

   rawgl::batch::BatchPrepareResult prepared = runner.prepare(workflow);
   if (!prepared.success) {
       // prepared.errorMessage
   }

   rawgl::batch::BatchJobHandle handle = runner.submit(*prepared.workflow);
   rawgl::batch::BatchResult result = handle.wait();

Python note
-----------

Python bindings exist for the batch surface. Name outputs explicitly when you
prepare batch workflows.

Do not rely on unnamed output inference on batch preparation paths.

Per-run outputs
---------------

Prepared batch workflows can write different output paths for each submitted
job. Construct the runner with an ``IoRuntime`` and pass named file outputs at
submit time. The workflow output must already be captured.

.. code-block:: python

   import rawgl

   session = rawgl.Session()
   io_runtime = rawgl.io.Runtime()
   runner = session.batch(io_runtime=io_runtime)

   workflow = rawgl.build_workflow(
       rawgl.image_pass(
           fragment_shader,
           size=(512, 512),
           inputs={"u_src0": first_array},
           outputs={
               "FrameOut": {
                   "format": "rgba32f",
                   "channels": 4,
                   "alpha_channel": 3,
                   "capture_to_host": True,
               }
           },
       ),
       verbosity=0,
   )

   prepared = rawgl.prepare_batch_workflow(runner, workflow)

   handle = prepared.submit(
       inputs={"u_src0": next_array},
       outputs={
           "FrameOut": {
               "path": "frames/frame_000.png",
               "format": "rgba32f",
               "channels": 4,
               "alpha_channel": 3,
               "bits": 16,
           }
       },
       system_uniforms={"frame": 0, "time": 0.0},
   )

   result = handle.wait()
   if not result.success:
       raise RuntimeError(result.error_message)

Use this shape for frame sequences, parameter sweeps, and production scripts
where one stable pass graph runs over many NumPy or file-backed entries.

Rules for per-run file outputs:

- the ``BatchRunner`` must be constructed with an ``IoRuntime``
- each requested file output must match a captured workflow output
- prepared file outputs and per-run file outputs are both saved
- one submitted job may save more than one named output

When to use it
--------------

Use ``BatchRunner`` when:

- you have many similar jobs to run
- you want explicit orchestration rather than host-side manual loops
- you want bounded in-flight behavior
- your workload benefits from separating preparation, execution, and save stages

Do not use it merely to run one workflow once. In that case, plain
``Session`` / ``PreparedWorkflow`` is simpler.
