..
  SPDX-License-Identifier: Apache-2.0

Concepts
========

Keep the layers separate.

That is the easiest way to reason about RawGL.

- describe the work declaratively
- prepare it once
- execute it many times with new inputs

RawGL is not intended to be an immediate-mode wrapper around OpenGL calls like
``clear``, ``draw``, or ``dispatch`` from the public Python or CLI surface.

Session
-------

``Session`` is the long-lived execution owner.

It owns reusable state such as:

- backend state
- shader and program caches
- prepared workflow reuse
- shared resource lifetime across repeated runs

For normal use, create one ``Session`` and reuse it across many workflow
preparations and executions.

Workflow lifecycle
------------------

The normal execution shape is:

1. create a ``Session``
2. build a ``Workflow``
3. prepare it into a ``PreparedWorkflow``
4. execute it with changing ``RunSettings``

That separation is what allows:

- cache reuse
- repeated execution without rebuilding everything
- a clear split between static graph structure and per-run values

Workflow
--------

``Workflow`` is the declarative description of the job.

A workflow contains passes, bindings, outputs, and pass-to-pass references. It
describes **what** should run, not the low-level OpenGL command sequence.

For common image-style render passes, a fragment-only pass can use the built-in
fullscreen quad vertex path automatically.

PreparedWorkflow
----------------

``PreparedWorkflow`` is the validated reusable form of a workflow.

Use it when:

- the pass topology stays fixed
- the shaders stay fixed
- only the per-run inputs change

This is the main path for repeated execution.

Think of ``PreparedWorkflow`` as the compiled reusable form of a job.

RunSettings and RunResult
-------------------------

``RunSettings`` contains the changing per-run values:

- input overrides
- host-memory images
- system uniform values

``RunResult`` contains:

- success/error state
- captured host outputs
- captured counters

``RunSettings`` should hold only what changes per execution. If changing
something requires rebuilding the workflow, it should not live in
``RunSettings``.

IoRuntime
---------

``IoRuntime`` owns the file-oriented layer.

It handles:

- loading files into ``HostImageData``
- rewriting file-backed workflow inputs to host-memory payloads
- saving captured outputs after execution

This matters especially for Python. A common Python path is:

- NumPy array in
- RawGL processing
- NumPy array out

without writing intermediate image files.

BatchRunner
-----------

``BatchRunner`` is the orchestration layer above ``Session`` and optional
``IoRuntime``.

It is the right place for:

- repeated submissions
- bounded queues and backpressure
- progress tracking
- save-stage overlap

It is not the same thing as ``Session``.

- ``Session`` owns execution state and prepared workflow reuse
- ``BatchRunner`` owns repeated submission and orchestration
