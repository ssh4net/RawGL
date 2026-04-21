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

When to use it
--------------

Use ``BatchRunner`` when:

- you have many similar jobs to run
- you want explicit orchestration rather than host-side manual loops
- you want bounded in-flight behavior
- your workload benefits from separating preparation, execution, and save stages

Do not use it merely to run one workflow once. In that case, plain
``Session`` / ``PreparedWorkflow`` is simpler.
