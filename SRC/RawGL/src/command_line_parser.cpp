// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "command_line_parser.h"

#include "common.h"
#include "opengl_utils.h"
#include "sequence.h"

#include <charconv>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <termcolor/termcolor.hpp>

namespace {

enum class ParsedOptionMode {
    flag,
    single,
    multi,
};

struct ParsedOptionSpec {
    const char* long_key = nullptr;
    char short_key       = '\0';
    ParsedOptionMode mode = ParsedOptionMode::flag;
};

static const ParsedOptionSpec RAWGL_OPTION_SPECS[] = {
    { "help", 'h', ParsedOptionMode::flag },
    { "version", 'v', ParsedOptionMode::flag },
    { "verbosity", 'V', ParsedOptionMode::single },
    { "pass_vertfrag", 'P', ParsedOptionMode::multi },
    { "pass_comp", 'C', ParsedOptionMode::single },
    { "pass_size", 'S', ParsedOptionMode::multi },
    { "pass_workgroupsize", 'W', ParsedOptionMode::multi },
    { "bg_color", '\0', ParsedOptionMode::multi },
    { "cull", '\0', ParsedOptionMode::multi },
    { "pass_mesh", 'M', ParsedOptionMode::multi },
    { "in", 'i', ParsedOptionMode::multi },
    { "atomic", 'B', ParsedOptionMode::multi },
    { "in_attr", 't', ParsedOptionMode::multi },
    { "out", 'o', ParsedOptionMode::multi },
    { "out_format", 'f', ParsedOptionMode::single },
    { "out_attr", 'r', ParsedOptionMode::multi },
    { "out_channels", 'n', ParsedOptionMode::single },
    { "out_alpha_channel", 'a', ParsedOptionMode::single },
    { "out_bits", 'b', ParsedOptionMode::single },
};

static const ParsedOptionSpec*
find_option_spec(const std::string& token)
{
    if (token.size() > 2 && token[0] == '-' && token[1] == '-') {
        const std::string optionName = token.substr(2);
        for (const ParsedOptionSpec& spec : RAWGL_OPTION_SPECS) {
            if (optionName == spec.long_key) {
                return &spec;
            }
        }
    } else if (token.size() == 2 && token[0] == '-') {
        for (const ParsedOptionSpec& spec : RAWGL_OPTION_SPECS) {
            if (spec.short_key != '\0' && token[1] == spec.short_key) {
                return &spec;
            }
        }
    }

    return nullptr;
}

static bool
is_option_token(const std::string& token)
{
    return find_option_spec(token) != nullptr;
}

static int
parse_option_int(const std::string& text, const char* optionName)
{
    int value                           = 0;
    const char* begin                   = text.data();
    const char* end                     = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);

    if (result.ec != std::errc() || result.ptr != end) {
        throw std::runtime_error(std::string("Invalid integer value for --") + optionName + ": " + text);
    }

    return value;
}

static std::string
build_help_text()
{
    std::ostringstream stream;
    stream << "Options\n"
           << "  --help, -h\n"
           << "  --version, -v\n"
           << "  --verbosity, -V <0-5>\n"
           << "  --pass_vertfrag, -P <file> [file]\n"
           << "  --pass_comp, -C <file>\n"
           << "  --pass_size, -S <X> [Y]\n"
           << "  --pass_workgroupsize, -W <X> [Y]\n"
           << "  --bg_color <R> [G] [B] [A]\n"
           << "  --cull <name value>...\n"
           << "  --pass_mesh, -M <quad|mesh> ...\n"
           << "  --in, -i <uniform> <value...>\n"
           << "  --atomic, -B <mode> <args...>\n"
           << "  --in_attr, -t <name> <value>\n"
           << "  --out, -o <name> <path>\n"
           << "  --out_format, -f <format>\n"
           << "  --out_attr, -r <name> <value>\n"
           << "  --out_channels, -n <count>\n"
           << "  --out_alpha_channel, -a <index>\n"
           << "  --out_bits, -b <bits>\n\n"
           << "Supported texture attributes:\n"
           << PassInput::get_possible_tex_attr_fmt() << '\n'
           << "Supported mesh attributes:\n"
           << MeshInput::get_possible_mesh_parm_fmt() << '\n'
           << "Supported culling attributes:\n"
           << Pass::get_possible_culling_fmt();
    return stream.str();
}

static void
print_version_text()
{
    std::cout << termcolor::bright_yellow << termcolor::bold;
    std::cout << APP_NAME << " version " << APP_VERSION[0] << "." << APP_VERSION[1] << "." << APP_VERSION[2]
              << " Copyright (c) " << APP_AUTHOR << std::endl;
    std::cout << "Build from: " << __DATE__ << ", " << __TIME__ << "." << std::endl;
    std::cout << termcolor::reset;
}

}  // namespace

SequenceParsedArguments
Sequence_ParseArguments(int argc, const char* argv[])
{
    SequenceParsedArguments parsed;

    for (int index = 1; index < argc; ++index) {
        const std::string token      = argv[index];
        const ParsedOptionSpec* spec = find_option_spec(token);

        if (spec == nullptr) {
            throw std::runtime_error("Unknown option: " + token);
        }

        if (spec->mode == ParsedOptionMode::flag) {
            if (std::string(spec->long_key) == "help") {
                parsed.showHelp = true;
            } else if (std::string(spec->long_key) == "version") {
                parsed.showVersion = true;
            }
            continue;
        }

        SequenceParsedOption option;
        option.string_key = spec->long_key;

        if (spec->mode == ParsedOptionMode::single) {
            if (index + 1 >= argc) {
                throw std::runtime_error("Missing value for option --" + option.string_key);
            }

            option.value.push_back(argv[++index]);
        } else {
            while (index + 1 < argc) {
                const std::string nextToken = argv[index + 1];
                if (is_option_token(nextToken)) {
                    break;
                }

                option.value.push_back(nextToken);
                ++index;
            }

            if (option.value.empty()) {
                throw std::runtime_error("Missing value for option --" + option.string_key);
            }
        }

        if (option.string_key == "verbosity") {
            parsed.verbosity = parse_option_int(option.value[0], "verbosity");
            continue;
        }

        parsed.options.push_back(std::move(option));
    }

    return parsed;
}

bool
Sequence_HandleImmediateParsedArguments(const SequenceParsedArguments& parsedArguments, int argc, int& exitCode)
{
    bool immediateExit = false;

    if (parsedArguments.showHelp || argc < 2) {
        std::cout << build_help_text() << std::endl;
        exitCode      = 0;
        immediateExit = true;
    }

    if (parsedArguments.showVersion) {
        print_version_text();
        get_GPUfeatures();
        exitCode      = 0;
        immediateExit = true;
    }

    return immediateExit;
}

bool
Sequence_HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode)
{
    const SequenceParsedArguments parsedArguments = Sequence_ParseArguments(argc, argv);
    return Sequence_HandleImmediateParsedArguments(parsedArguments, argc, exitCode);
}
