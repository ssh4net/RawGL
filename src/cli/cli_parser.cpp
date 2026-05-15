// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "cli_parser.h"

#include "common.h"
#include "gl_utils.h"
#include "runtime_help_text.h"
#include "rawgl/rawgl_core.h"

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
    { "doctor", '\0', ParsedOptionMode::flag },
    { "gl_platform", '\0', ParsedOptionMode::single },
    { "verbosity", 'V', ParsedOptionMode::single },
    { "pass_vertfrag", 'P', ParsedOptionMode::multi },
    { "pass_comp", 'C', ParsedOptionMode::single },
    { "pass_size", 'S', ParsedOptionMode::multi },
    { "pass_workgroupsize", 'W', ParsedOptionMode::multi },
    { "bg_color", '\0', ParsedOptionMode::multi },
    { "cull", '\0', ParsedOptionMode::multi },
    { "pass_mesh", 'M', ParsedOptionMode::multi },
    { "in", 'i', ParsedOptionMode::multi },
    { "in_backend", '\0', ParsedOptionMode::single },
    { "in_jpeg_color_transform", '\0', ParsedOptionMode::single },
    { "in_png_expand_transparency", '\0', ParsedOptionMode::single },
    { "in_tiff_directory_index", '\0', ParsedOptionMode::single },
    { "in_exr_channels", '\0', ParsedOptionMode::single },
    { "in_jpeg2000_reduce_factor", '\0', ParsedOptionMode::single },
    { "in_jpeg2000_layer_limit", '\0', ParsedOptionMode::single },
    { "atomic", 'B', ParsedOptionMode::multi },
    { "in_attr", 't', ParsedOptionMode::multi },
    { "out", 'o', ParsedOptionMode::multi },
    { "out_format", 'f', ParsedOptionMode::single },
    { "out_attr", 'r', ParsedOptionMode::multi },
    { "out_channels", 'n', ParsedOptionMode::single },
    { "out_alpha_channel", 'a', ParsedOptionMode::single },
    { "out_bits", 'b', ParsedOptionMode::single },
    { "out_jpeg_quality", '\0', ParsedOptionMode::single },
    { "out_jpeg_progressive", '\0', ParsedOptionMode::single },
    { "out_jpeg_optimize", '\0', ParsedOptionMode::single },
    { "out_jpeg_subsampling", '\0', ParsedOptionMode::single },
    { "out_png_compression", '\0', ParsedOptionMode::single },
    { "out_png_interlace", '\0', ParsedOptionMode::single },
    { "out_tiff_compression", '\0', ParsedOptionMode::single },
    { "out_tiff_predictor", '\0', ParsedOptionMode::single },
    { "out_tiff_layout", '\0', ParsedOptionMode::single },
    { "out_tiff_tile_size", '\0', ParsedOptionMode::multi },
    { "out_tiff_rows_per_strip", '\0', ParsedOptionMode::single },
    { "out_tiff_big_tiff", '\0', ParsedOptionMode::single },
    { "out_tiff_unassociated_alpha", '\0', ParsedOptionMode::single },
    { "out_tiff_jpeg_quality", '\0', ParsedOptionMode::single },
    { "out_tiff_deflate_level", '\0', ParsedOptionMode::single },
    { "out_tiff_zstd_level", '\0', ParsedOptionMode::single },
    { "out_tiff_lzma_preset", '\0', ParsedOptionMode::single },
    { "out_tiff_webp_level", '\0', ParsedOptionMode::single },
    { "out_tiff_webp_lossless", '\0', ParsedOptionMode::single },
    { "out_tiff_webp_lossless_exact", '\0', ParsedOptionMode::single },
    { "out_exr_compression", '\0', ParsedOptionMode::single },
    { "out_exr_layout", '\0', ParsedOptionMode::single },
    { "out_exr_tile_size", '\0', ParsedOptionMode::multi },
    { "out_exr_line_order", '\0', ParsedOptionMode::single },
    { "out_exr_dwa_level", '\0', ParsedOptionMode::single },
    { "out_jpeg2000_lossless", '\0', ParsedOptionMode::single },
    { "out_jpeg2000_compression_ratio", '\0', ParsedOptionMode::single },
    { "out_jpeg2000_quality", '\0', ParsedOptionMode::single },
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
           << "  --doctor\n"
           << "  --gl_platform <auto|x11|wayland>\n"
           << "  --verbosity, -V <0-5>\n"
           << "  --pass_vertfrag, -P <file> [file]\n"
           << "  --pass_comp, -C <file>\n"
           << "  --pass_size, -S <X> [Y]\n"
           << "  --pass_workgroupsize, -W <X> [Y]\n"
           << "  --bg_color <R> [G] [B] [A]\n"
           << "  --cull <name value>...\n"
           << "  --pass_mesh, -M <quad|mesh> ...\n"
           << "  --in, -i <uniform> <value...>\n"
           << "  --in_backend <auto|native|native_only|openimageio|openimageio_only>\n"
           << "  --in_jpeg_color_transform <auto|rgb|grayscale>\n"
           << "  --in_png_expand_transparency <true|false>\n"
           << "  --in_tiff_directory_index <index>\n"
           << "  --in_exr_channels <auto|luminance|rgb|rgba|all>\n"
           << "  --in_jpeg2000_reduce_factor <levels>\n"
           << "  --in_jpeg2000_layer_limit <layers>\n"
           << "  --atomic, -B <mode> <args...>\n"
           << "  --in_attr, -t <name> <value>\n"
           << "  --out, -o <name> <path>\n"
           << "  --out_format, -f <format>\n"
           << "  --out_attr, -r <name> <value>\n"
           << "  --out_channels, -n <count>\n"
           << "  --out_alpha_channel, -a <index>\n"
           << "  --out_bits, -b <bits>\n"
           << "  --out_jpeg_quality <1-100>\n"
           << "  --out_jpeg_progressive <true|false>\n"
           << "  --out_jpeg_optimize <true|false>\n"
           << "  --out_jpeg_subsampling <default|444|422|420|440|411>\n"
           << "  --out_png_compression <0-9>\n"
           << "  --out_png_interlace <true|false>\n"
           << "  --out_tiff_compression <none|lzw|packbits|deflate|jpeg|zstd|...>\n"
           << "  --out_tiff_predictor <none|horizontal|float>\n"
           << "  --out_tiff_layout <strips|tiled>\n"
           << "  --out_tiff_tile_size <width> [height]\n"
           << "  --out_tiff_rows_per_strip <rows>\n"
           << "  --out_tiff_big_tiff <true|false>\n"
           << "  --out_tiff_unassociated_alpha <true|false>\n"
           << "  --out_tiff_jpeg_quality <1-100>\n"
           << "  --out_tiff_deflate_level <1-9>\n"
           << "  --out_tiff_zstd_level <1-22>\n"
           << "  --out_tiff_lzma_preset <0-9>\n"
           << "  --out_tiff_webp_level <0-100>\n"
           << "  --out_tiff_webp_lossless <true|false>\n"
           << "  --out_tiff_webp_lossless_exact <true|false>\n"
           << "  --out_exr_compression <none|rle|zips|zip|piz|dwaa|dwab|...>\n"
           << "  --out_exr_layout <scanlines|tiled>\n"
           << "  --out_exr_tile_size <width> [height]\n"
           << "  --out_exr_line_order <increasing_y|decreasing_y|random_y>\n"
           << "  --out_exr_dwa_level <float>\n"
           << "  --out_jpeg2000_lossless <true|false>\n"
           << "  --out_jpeg2000_compression_ratio <float>\n"
           << "  --out_jpeg2000_quality <float>\n\n"
           << "Supported texture attributes:\n"
           << GetTextureAttributeHelpText() << '\n'
           << "Supported mesh attributes:\n"
           << GetMeshParameterHelpText() << '\n'
           << "Supported culling attributes:\n"
           << GetCullingHelpText();
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

static void
print_runtime_info(const rawgl::RuntimeInfo& info)
{
    std::cout << "RawGL runtime diagnostics\n";
    std::cout << "  Status: " << (info.success ? "ok" : "failed") << '\n';
    if (!info.errorMessage.empty()) {
        std::cout << "  Error: " << info.errorMessage << '\n';
    }
    std::cout << "  Requested platform: " << info.requestedPlatform << '\n';
    std::cout << "  Selected platform: " << info.selectedPlatform << '\n';
    std::cout << "  DISPLAY: " << info.display << '\n';
    std::cout << "  WAYLAND_DISPLAY: " << info.waylandDisplay << '\n';
    if (info.success) {
        std::cout << "  OpenGL vendor: " << info.vendor << '\n';
        std::cout << "  OpenGL renderer: " << info.renderer << '\n';
        std::cout << "  OpenGL version: " << info.version << '\n';
        std::cout << "  GLSL version: " << info.shadingLanguageVersion << '\n';
        std::cout << "  Software renderer: " << (info.softwareRenderer ? "yes" : "no") << '\n';
        if (info.softwareRenderer) {
            std::cout << "  Warning: RawGL is running through a software OpenGL renderer.\n";
        }
    }
}

}  // namespace

CommandLineParsedArguments
ParseCommandLineArguments(int argc, const char* argv[])
{
    CommandLineParsedArguments parsed;

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
            } else if (std::string(spec->long_key) == "doctor") {
                parsed.showDoctor = true;
            }
            continue;
        }

        CommandLineParsedOption option;
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

        if (option.string_key == "gl_platform") {
            parsed.glPlatform    = option.value[0];
            parsed.hasGlPlatform = true;
            continue;
        }

        parsed.options.push_back(std::move(option));
    }

    return parsed;
}

bool
HandleImmediateParsedArguments(const CommandLineParsedArguments& parsedArguments, int argc, int& exitCode)
{
    bool immediateExit = false;

    if (parsedArguments.showHelp || argc < 2) {
        std::cout << build_help_text() << std::endl;
        exitCode      = 0;
        immediateExit = true;
    }

    if (parsedArguments.showVersion) {
        print_version_text();
        exitCode      = 0;
        immediateExit = true;
    }

    if (parsedArguments.showDoctor) {
        const rawgl::RuntimeInfo info = rawgl::ProbeRuntimeInfo();
        print_runtime_info(info);
        exitCode      = info.success ? 0 : 1;
        immediateExit = true;
    }

    return immediateExit;
}

void
ApplyParsedRuntimeOptions(const CommandLineParsedArguments& parsedArguments)
{
    if (parsedArguments.hasGlPlatform) {
        rawgl_set_opengl_platform_override(parsedArguments.glPlatform.c_str());
    }
}

bool
HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode)
{
    const CommandLineParsedArguments parsedArguments = ParseCommandLineArguments(argc, argv);
    ApplyParsedRuntimeOptions(parsedArguments);
    return HandleImmediateParsedArguments(parsedArguments, argc, exitCode);
}
