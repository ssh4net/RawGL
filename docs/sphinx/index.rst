..
  SPDX-License-Identifier: Apache-2.0

RawGL
=====

RawGL is a batch-oriented OpenGL Shading Language (GLSL) processing engine. It
describes work as workflows instead of exposing direct OpenGL command
scripting.

RawGL has three main entry points:

- the command-line interface (CLI) for file-oriented processing
- Python for NumPy-driven processing
- C++ for embedding RawGL in a larger application

RawGL also exposes two support layers:

- ``rawgl::io::IoRuntime`` for file loading and saving
- ``rawgl::batch::BatchRunner`` for repeated job submission

Choose your entry point by workload:

- use :doc:`quick_start` to get one workflow running quickly
- use :doc:`examples` for example-driven workflow shapes
- use :doc:`python` for NumPy/scikit/OpenCV-driven processing
- use :doc:`cli` for file-oriented scripted pipelines
- use :doc:`cpp` for embedding RawGL in a larger C++ application
- use :doc:`io` and :doc:`batch` when you need file materialization or orchestration explicitly

Main public types:

- ``Session`` for reusable execution state
- ``Workflow`` for a declarative pass graph
- ``PreparedWorkflow`` for validated reusable execution
- ``RunSettings`` and ``RunResult`` for per-run data
- ``IoRuntime`` for file decode/encode materialization
- ``BatchRunner`` for orchestration

This site is the user guide. The engineering notes in the repository
``docs/`` directory are still useful, but they are not the main documentation.

.. toctree::
   :maxdepth: 2
   :caption: User Guide

   quick_start
   examples
   concepts
   cpp
   python
   cli
   io
   batch
   api
