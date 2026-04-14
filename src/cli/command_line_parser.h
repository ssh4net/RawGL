// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <string>
#include <vector>

struct SequenceParsedOption {
    std::string string_key;
    std::vector<std::string> value;
};

struct SequenceParsedArguments {
    bool showHelp    = false;
    bool showVersion = false;
    int verbosity    = 3;
    std::vector<SequenceParsedOption> options;
};

SequenceParsedArguments
Sequence_ParseArguments(int argc, const char* argv[]);

bool
Sequence_HandleImmediateParsedArguments(const SequenceParsedArguments& parsedArguments, int argc, int& exitCode);

bool
Sequence_HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode);
