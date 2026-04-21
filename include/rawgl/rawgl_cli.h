// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

namespace rawgl {

/// Request for the top-level RawGL command-line frontend.
struct CommandLineRequest {
    int argc = 0;
    const char** argv = nullptr;
};

/// Result returned by the top-level RawGL command-line frontend.
struct CommandLineResult {
    /// Process exit code that should be returned to the caller.
    int exitCode = 0;
    /// True when the command line requested an immediate action such as help or version.
    bool immediateExit = false;
    /// True when a workflow was prepared and executed.
    bool executed = false;
};

/// Runs the RawGL command-line frontend.
CommandLineResult
Run(const CommandLineRequest& request);

/// Legacy argv-style command-line entry point.
int
RunCommandLine(int argc, const char* argv[]);

}  // namespace rawgl
