// SPDX-License-Identifier: LGPL-2.1-or-later
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
    int verbosity    = 3;
    std::vector<CommandLineParsedOption> options;
};

CommandLineParsedArguments
ParseCommandLineArguments(int argc, const char* argv[]);

bool
HandleImmediateParsedArguments(const CommandLineParsedArguments& parsedArguments, int argc, int& exitCode);

bool
HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode);
