// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <string>
#include <vector>

struct CommandLineParsedOption {
    std::string string_key;
    std::vector<std::string> value;
};

struct CommandLineParsedArguments {
    bool showHelp    = false;
    bool showVersion = false;
    bool showDoctor  = false;
    bool hasGlPlatform = false;
    std::string glPlatform;
    int verbosity    = 3;
    std::vector<CommandLineParsedOption> options;
};

CommandLineParsedArguments
ParseCommandLineArguments(int argc, const char* argv[]);

bool
HandleImmediateParsedArguments(const CommandLineParsedArguments& parsedArguments, int argc, int& exitCode);

void
ApplyParsedRuntimeOptions(const CommandLineParsedArguments& parsedArguments);

bool
HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode);
