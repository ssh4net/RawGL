/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Sequence.h"
#include "Timer.h"
#include "Log.h"
#include "GLProgramManager.h"

#include <boost/program_options.hpp>
#include <boost/program_options/option.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/program_options/value_semantic.hpp>

namespace po = boost::program_options;

// we don't use positional options, so this is ok
static std::vector<po::option> ignore_numbers(std::vector<std::string>& args)
{
    std::vector<po::option> result;
    int pos = 0;

    while (!args.empty()) {
        const auto& arg = args[0];
        double num;

        if (boost::conversion::try_lexical_convert(arg, num)) {
            result.push_back(po::option());
            po::option& opt = result.back();

            opt.position_key = pos++;
            opt.value.push_back(arg);
            opt.original_tokens.push_back(arg);

            args.erase(args.begin());
        }

        break;
    }

    return result;
}

namespace std
{
    std::ostream& operator<<(std::ostream& os, const std::vector<std::string>& vec)
    {
        for (auto item : vec)
            os << item << " ";

        return os;
    }
}

Sequence::Sequence(int argc, const char* argv[]) :
    Sequence()
{
    try {
        // TODO: Make defaulted options work outside of po::variables_map

        po::options_description desc{ "Options" };
        desc.add_options()
            ("help,h", "Show help message.")
            ("version,v", "Show program version.")

            ("verbosity,V", po::value<int>()->default_value(3),
                "Log level (selection & above will be shown):\n"
				" 0 - fatal error only\n"
                " 1 - errors only\n"
                " 2 - warnings only\n"
                " 3 - info\n"
				" 4 - debug\n"
				" 5 - trace\n")
            ("pass_vertfrag,P", po::value<std::vector<std::string>>()->multitoken(),
                "New pass using vertex & fragment shaders (in GLSL or SPIR-V format):\n"
                " --pass_vertfrag s.vertfrag\n"
                "  (sources separated with macros RAWGL_VERTEX_SHADER, RAWGL_FRAGMENT_SHADER)\n"
                " --pass_vertfrag s.vert_spv s.frag_spv")
            ("pass_comp,C", po::value<std::vector<std::string>>(),
                "New pass using a compute shader:\n"
                " --pass_comp s.comp")
            ("pass_size,S", po::value<std::vector<std::string>>()->multitoken()->default_value(std::vector<std::string>{ "512" }),
                "Output size of this pass:\n"
                " --pass_size X [Y]\n"
                "X and Y can also reference the size of an input texture from any pass:\n"
                " --pass_size Texture0::0 [Texture1::1]")
            ("pass_workgroupsize,W", po::value<std::vector<std::string>>()->multitoken()->default_value(std::vector<std::string>{ "1" }),
                "Number of threads per work group on each axis:\n"
                " --pass_workgroupsize X [Y]\n"
                "Must be equal to the 'local_size' layout constant inside compute shader.")
            ("in,i", po::value<std::vector<std::string>>()->multitoken(),
                (
                    "Uniform pass index, name & value\n"
                    "(numeric or texture path)\n"
                    "(e.g.: --in Texture0 BasicTex.png).\n"
                    "as output from #-pass: --in outTexture 0\n"
                    ""
                    "Texture filtering and sampling:\n"
                    + PassInput::get_possible_tex_attr_fmt()
                    ).c_str()
                )
            ("in_attr,t", po::value<std::vector<std::string>>()->multitoken(), "OpenImageIO/plugin attribute value (e.g.: --in_attr oiio:colorspace sRGB).")
            /*
                        ("in_color_space,s", po::value<std::vector<std::string>>(), "Color space of input images (e.g.: --in_color_space raw).")
                        ("in_demosaic,d", po::value<std::vector<std::string>>(), "Demosaic algorithm for input images (e.g.: --in_demosaic AAHD.")
                        ("in_user_flip,f", po::value<std::vector<int>>(),
                            "Flip value for RAW input images.\n"
                            "-1: none (default)\n"
                            "0-8: FILL THIS!!!"
                        )
            */
            ("out,o", po::value<std::vector<std::string>>()->multitoken(),
                "Shader output channel pass, name & path to save the processed image (e.g.: --out OutColor output.jpg).\n"
                "Must have a file name and extension, the extension will determine the ability to save the file with the necessary options.\n"
                "The file will be recognized as:\n"
                "BMP:      *.bmp\n"
                "PNG:      *.png\n"
                "JPEG:     *.jpg, *.jpe, *.jpeg, *.jif, *.jfif, *.jfi\n"
                "Targa:    *.tga, *.tpic\n"
                "OpenEXR:  *.exr\n"
                "HDR/RGBE: *.hdr\n"
                "TIFF:     *.tif, *.tiff, *.tx, *.env, *.sm, *.vsm")
            ("out_format,f", po::value<std::vector<std::string>>()->default_value({ "rgba32f" }),
                "Output framebuffer format:\n"
                " r8, rg8, rgb8, rgba8,\n"
                " r16, rg16, rgb16, rgba16,\n"
                " r16f, rg16f, rgb16f, rgba16f,\n"
                " r32f, rg32f, rgb32f, rgba32f")
            ("out_attr,r", po::value<std::vector<std::string>>()->multitoken(), "OpenImageIO/plugin attribute value (e.g.: --out_attr oiio:colorspace sRGB).")
            ("out_channels,n", po::value<std::vector<int>>(), "# of channels in output image")
            ("out_alpha_channel,a", po::value<std::vector<int>>(), "Alpha channel index hint for output image (-1 = off, 0-3 = RGBA).")
            ("out_bits,b", po::value<std::vector<int>>(),
                "# of bits per output image channel\n"
                "(depends on file format):\n"
                "BMP:      8\n"
                "PNG:      8, 16\n"
                "JPEG:     8 \n"
                "Targa:    8, 16\n"
                "OpenEXR:  16, 32 (half & float)\n"
                "HDR/RGBE: 32\n"
                "TIFF:     8, 16, 32 float")
            /*
                        ("out_compression,c", po::value<std::vector<std::string>>()->multitoken(),
                            "Output image file compression (e.g.: --out_compression JPEG 100)\n"
                            "(depends on file format, will be silently ignored if it doesn't meet the file format requirements):\n"
                            "BMP:      compression not supported\n"
                            "PNG:      png [0 - 9], default is 6\n"
                            "JPEG:     jpeg [0 - 100]\n"
                            "Targa:    none | rle\n"
                            "OpenEXR:  none | rle | zip | zips | piz | pxr24 | b44 | b44a "
                            "| dwaa | dwab : [value]\n"
                            "HDR/RGBE: ?\n"
                            "TIFF:     much more, see online link)\n"
                            "More details on the link\n"
                            "https://openimageio.readthedocs.io/en/master/builtinplugins.html")
            */
            ;

        //
        // parse options
        //

        auto parsedOptions = po::command_line_parser(argc, argv).extra_style_parser(&ignore_numbers).options(desc).run();

        // store the options in a map (although we don't use it to access the multitoken ones below, doing it directly instead)
        po::variables_map vm;
        po::store(parsedOptions, vm);
        //po::store(po::command_line_parser(argc, argv).options(desc).positional(positional_opt_desc).run(), vm);
        po::notify(vm);

        // for --help, -h, --version, -v command line arguments just print info and exit
        bool infoExit = false;

        if (vm.count("help") || argc < 2)
        {
            std::cout << desc << std::endl;
            infoExit = true;
        }

        if (vm.count("version"))
        {
            std::cout << APP_NAME << " version " << APP_VERSION[0] << "." << APP_VERSION[1] << " (build from " << __DATE__ << ", " << __TIME__ << ")." << std::endl;
            infoExit = true;
        }

        if (infoExit)
            exit(1);

        // set the logger verbosity
		Log_SetVerbosity(std::clamp(vm["verbosity"].as<int>(), 0, 5));

        //
        // parse the multi-occurence, multi-token options here
        //

        Pass* currentPass = nullptr;
        PassInput* currentInput = nullptr;
        PassOutput* currentOutput = nullptr;

        int currentPassN = 0;

        // tired. Don't know correct way to store previous output parameters
        std::string p_internalFormatText = "rgb32f";
        int p_channels = 3;
        int p_alphaChannel = -1;
        int p_bits = 16;

        for (auto& o : parsedOptions.options)
        {
            //
            // Pass
            //
			
            if (o.string_key == "pass_vertfrag" || o.string_key == "pass_comp")
            {
                // Start a new pass
                currentPassN = m_passes.size();
                LOG(debug) << "Loading pass " << currentPassN;

                std::shared_ptr<GLProgram> program;

                if (o.string_key == "pass_vertfrag")
                {
                    if (o.value.size() == 1)
                    {
                        // Single text file
                        program = g_glslProgramManager.loadVertFrag(o.value[0]);
                    }
                    else
                    {
                        // Two text/binary files
                        std::string s[]{ o.value[0], o.value[1] };
                        program = g_glslProgramManager.loadVertFrag(s);
                    }
                }
                else
                {
                    // Single text/binary file
                    program = g_glslProgramManager.loadComp(o.value[0]);
                }

                if (!program->isValid())
                {
                    LOG(error) << "Failed to load program for the pass.";
                    exit(1);
                }

                m_passes.push_back(Pass(program, o.string_key == "pass_comp"));

                // reset the pointers
                currentPass = &m_passes.back();
                currentInput = nullptr;
                currentOutput = nullptr;

                // Gather all outputs to work with them further a bit later
                //for ()
                // 
                // Set pass_size from previous pass
                if (currentPassN > 0) {
                    currentPass->sizeText[0] = m_passes[currentPassN - 1].sizeText[0];
                    currentPass->sizeText[1] = m_passes[currentPassN - 1].sizeText[1];
                }

                continue;
            }

            //
            // generic parameters
            //

            if (o.string_key == "pass_size")
            {
                if (!currentPass)
                {
                    LOG(error) << "pass_size: no preceeding pass declaration.";
                    exit(1);
                }

                // specify input data (for GLSL uniform)
                if (o.value.size() < 1 || o.value.size() > 2)
                {
                    LOG(error) << "pass_size: must have 1 or 2 parameters.";
                    exit(1);
                }

                currentPass->sizeText[0] = o.value[0];
                currentPass->sizeText[1] = o.value.size() > 1 ? o.value[1] : currentPass->sizeText[0];
            }

            if (o.string_key == "pass_workgroupsize")
            {
                if (!currentPass)
                {
                    LOG(error) << "pass_workgroupsize: no preceeding pass declaration.";
                    exit(1);
                }

                if (!currentPass->isCompute)
                {
                    LOG(error) << "pass_workgroupsize: not a compute pass.";
                    exit(1);
                }

                // specify input data (for GLSL uniform)
                if (o.value.size() < 1 || o.value.size() > 2)
                {
                    LOG(error) << "pass_workgroupsize: must have 1 or 2 parameters.";
                    exit(1);
                }

                currentPass->workGroupSizeText[0] = o.value[0];
                currentPass->workGroupSizeText[1] = o.value.size() > 1 ? o.value[1] : "1";
                //currentPass->workGroupSizeText[2] = o.value.size() > 2 ? o.value[2] : "1";
            }

            //
            // input & its parameters
            //

            if (o.string_key == "in")
            {
                if (!currentPass)
                {
                    LOG(error) << "in (" << o.value[0] << "): no preceeding pass declaration.";
                    exit(1);
                }

                // specify input data (for GLSL uniform)
                if (o.value.size() < 2)
                {
                    LOG(error) << "in (" << o.value[0] << "): must have at least 2 parameters - uniform, value(s).";
                    exit(1);
                }

#if 0
                // get pass index
                int passIndex = str_to_numeric<int>(o.value[0]);

                if (passIndex < 0 || passIndex >= m_passes.size())
                {
                    LOG(error) << "in (" << o.value[0] << "): pass index " << passIndex << " out of range.";
                    exit(1);
                }

                auto& pass = m_passes[passIndex];
#endif

                // get uniform reference
                auto shaderUniform = currentPass->program->findUniform(o.value[0]);

                if (!shaderUniform)
                {
                    LOG(error) << "in (" << o.value[0] << "): program uniform not found.";
                    continue;
                    //exit(1);
                }

                // link the input to its uniform
                auto [inputIt, success] = currentPass->inputs.insert({ o.value[0], PassInput() });

                if (!success)
                {
                    LOG(error) << "in (" << o.value[0] << "): duplicate id.";
                    exit(1);
                }

                currentInput = &inputIt->second;
                currentInput->uniform = shaderUniform;

                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(o.value.size() - 1);
                std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                const GLenum uniform_type = currentInput->uniform->type;

                // Image / sampler values
                if (uniform_type == GL_SAMPLER_2D || uniform_type == GL_IMAGE_2D) {

                    for (size_t i = 0; i < val_arr.size(); ++i) {
                        std::string val_key = val_arr[i];
                        std::string val_data;

                        if (i < val_arr.size() - 1) {
                            val_data = val_arr[i + 1];
                        }
                        // if val_data is empty this mean no previous pass values or hr_attributes
                        if (val_data == "") {
                            currentInput->path = val_key;
                            continue;
                        }
                        // Search for texture attributes
                        hres hr_tex_attr = hres::OK;
                        currentInput->eval_tex_attr(hr_tex_attr, val_key, val_data);

                        if (hres::OK == hr_tex_attr) {
                            i++; // Jump over next iteration, it contains only value which was already read
                        }
                        else {
                            // Search for value from previous passes.
                            hres hr_convert = hres::OK;
                            const int32_t ref_pass_index = str_to_numeric<int>(hr_convert, val_data);

                            if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                                auto& ref_pass = m_passes[ref_pass_index];
                                auto ref_output_it = ref_pass.outputs.find(val_key);

                                if (ref_output_it == ref_pass.outputs.end()) {
                                    ref_output_it = ref_pass.outputs.insert({ val_key, PassOutput() }).first;
                                }

                                auto& ref_output = ref_output_it->second;

                                hres hr_find_uniform = hres::OK;

                                if (ref_pass.isCompute) {
                                    ref_output.uniform = ref_pass.program->findUniform(val_key);

                                    if (!ref_output.output) {
                                        hr_find_uniform = hres::ERR;
                                    }
                                }
                                else {
                                    ref_output.output = ref_pass.program->findOutput(val_key);

                                    if (!ref_output.output) {
                                        hr_find_uniform = hres::ERR;
                                    }
                                }

                                if (hres::OK == hr_find_uniform) {
                                    i++; // Jump over next iteration, it contains only value which was already read
                                }
                                else {
                                    LOG(error) << "in (" << o.value[0] << "): referenced program output " << val_data.c_str() << " not found.";
                                    exit(1);
                                }
                                //hacky fix for previous pass output 
                                currentInput->path = (val_key + "::" + val_data);
                            }
                            else {
                                // no usable hr_attrib was found
                                currentInput->path = val_key;
                            }
                        }
                    }
                }
                else { // Numeric values
                    uint8_t num_fields = 1;
                    bool is_floats = false;

                    switch (currentInput->uniform->type) {
                    case GL_BOOL:
                    case GL_INT:
                        //is_floats = false;
                        num_fields = 1;
                        break;
                    case GL_BOOL_VEC2:
                    case GL_INT_VEC2:
                        //is_floats = false;
                        num_fields = 2;
                        break;
                    case GL_BOOL_VEC3:
                    case GL_INT_VEC3:
                        //is_floats = false;
                        num_fields = 3;
                        break;
                    case GL_BOOL_VEC4:
                    case GL_INT_VEC4:
                        //is_floats = false;
                        num_fields = 4;
                        break;

                    case GL_FLOAT:
                        is_floats = true;
                        num_fields = 1;
                        break;
                    case GL_FLOAT_VEC2:
                        is_floats = true;
                        num_fields = 2;
                        break;
                    case GL_FLOAT_VEC3:
                        is_floats = true;
                        num_fields = 3;
                        break;
                    case GL_FLOAT_VEC4:
                    case GL_FLOAT_MAT2:
                        is_floats = true;
                        num_fields = 4;
                        break;
                    case GL_FLOAT_MAT2x3:
                    case GL_FLOAT_MAT3x2:
                        is_floats = true;
                        num_fields = 6;
                        break;
                    case GL_FLOAT_MAT2x4:
                    case GL_FLOAT_MAT4x2:
                        is_floats = true;
                        num_fields = 8;
                        break;
                    case GL_FLOAT_MAT3:
                        is_floats = true;
                        num_fields = 9;
                        break;
                    case GL_FLOAT_MAT3x4:
                    case GL_FLOAT_MAT4x3:
                        is_floats = true;
                        num_fields = 12;
                        break;
                    case GL_FLOAT_MAT4:
                        is_floats = true;
                        num_fields = 16;
                        break;
                    default:
                        hr = hres::ERR;
                        break;
                    }

                    if (val_arr.size() < num_fields) {
                        hr = hres::ERR;
                        LOG(error) << "Value is missing values: " << val_arr.size() << '/' << num_fields;
                        return;
                    }

                    GLint tmp_ints[PassInput::NUM_INTS] = { 0 };
                    GLfloat tmp_floats[PassInput::NUM_FLOATS] = { 0.0f };

                    for (uint8_t i = 0; i < num_fields; ++i) {
                        const std::string& str_val = val_arr[i];

                        if (is_floats) {
                            tmp_floats[i] = str_to_numeric<float_t>(hr, str_val);
                        }
                        else {
                            tmp_ints[i] = str_to_numeric<int32_t>(hr, str_val);
                        }
                    }

                    if (hres::OK == hr) {
                        if (is_floats) {
                            memcpy(&currentInput->floats, &tmp_floats, sizeof(GLfloat) * PassInput::NUM_FLOATS);
                        }
                        else {
                            memcpy(&currentInput->ints, &tmp_ints, sizeof(GLint) * PassInput::NUM_INTS);
                        }
                    }
                }

            }
            else if (o.string_key == "in_attr")
            {
                if (!currentInput)
                {
                    LOG(error) << "in_attr (" << o.value[0] << "): no preceeding input declaration.";
                    exit(1);
                }

                switch (currentInput->uniform->type)
                {
                case GL_SAMPLER_2D:
                case GL_IMAGE_2D:
                    break;
                default:
                    LOG(error) << "in_attr (" << o.value[0] << "): attributes can only be specified for a texture input.";
                    exit(1);
                }

                if (o.value.size() < 2)
                {
                    LOG(error) << "in_attr (" << o.value[0] << "): must have 2 parameters.";
                    exit(1);
                }

                currentInput->attributes[o.value[0]] = o.value[1];
            }

            //
            // output & its parameters
            //

            if (o.string_key == "out")
            {
                if (!currentPass)
                {
                    LOG(error) << "out (" << o.value[0] << "): no preceeding pass declaration.";
                    exit(1);
                }

                // configure output file path
                if (o.value.size() != 2)
                {
                    LOG(error) << "out (" << o.value[0] << "): must have 2 parameters.";
                    exit(1);
                }

                // Unlike the input reference based creation,
                // here we can omit checking if the pass output already exists,
                // and check for a duplicate instead.
                auto [outputIt, success] = currentPass->outputs.insert({ o.value[0], PassOutput() });

                if (!success)
                {
                    // can only configure program output once per pass
                    LOG(error) << "out (" << o.value[0] << "): duplicate id.";
                    exit(1);
                }

                currentOutput = &outputIt->second;

                // Set pass_size from previous pass
                if (currentPassN > 0) {
                    currentOutput->internalFormatText = p_internalFormatText;
                    currentOutput->channels = p_channels;
                    currentOutput->alphaChannel = p_alphaChannel;
                    currentOutput->bits = p_bits;
                }
				
                // TODO: Check for output path duplicates using a pass-wide map.
                currentOutput->path = o.value[1];

                bool fail = false;

                if (currentPass->isCompute)
                {
                    // link it to the program output uniform
                    currentOutput->uniform = currentPass->program->findUniform(o.value[0]);

                    if (!currentOutput->uniform)
                        fail = true;
                }
                else
                {
                    // link it to the program output
                    currentOutput->output = currentPass->program->findOutput(o.value[0]);

                    if (!currentOutput->output)
                        fail = true;
                }

                if (fail)
                {
                    LOG(error) << "out (" << o.value[0] << "): program output not found.";
                    exit(1);
                }

            }
            else if (o.string_key == "out_channels")
            {
                if (!currentOutput)
                {
                    LOG(error) << "out_channels (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->channels = str_to_numeric<int32_t>(o.value[0]);
                p_channels = currentOutput->channels;
            }
            else if (o.string_key == "out_alpha_channel")
            {
                if (!currentOutput)
                {
                    LOG(error) << "out_alpha_channel (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->alphaChannel = str_to_numeric<int32_t>(o.value[0]);

                if (currentOutput->alphaChannel > 3)
                {
                    LOG(error) << "out_alpha_channel (" << o.value[0] << "): index > 3 is unsupported.";
                    exit(1);
                }

                p_alphaChannel = currentOutput->alphaChannel;
            }
            else if (o.string_key == "out_bits")
            {
                if (!currentOutput)
                {
                    LOG(error) << "out_bits (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->bits = str_to_numeric<int32_t>(o.value[0]);
                p_bits = currentOutput->bits;
            }
            else if (o.string_key == "out_format")
            {
                if (!currentOutput)
                {
                    LOG(error) << "out_format (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->internalFormatText = o.value[0];
                p_internalFormatText = o.value[0];
            }
            else if (o.string_key == "out_attr")
            {
                if (!currentOutput)
                {
                    LOG(error) << "out_attr (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }
                /*
                        // Output is always a file
                        switch(currentOutput->uniform->type)
                                {
                        case GL_SAMPLER_2D:
                            break;
                        default:
                                    LOG(info) << "out_attr (%s): attributes can only be specified for a file output.\n", o.value[0].c_str());
                                    exit(1);
                                }
                */
                if (o.value.size() < 2)
                {
                    LOG(error) << "out_attr (" << o.value[0] << "): must have 2 parameters.";
                    exit(1);
                }

                currentOutput->attributes[o.value[0]] = o.value[1];
            }
        }
    }
    catch (const po::required_option& e)
    {
        // required options missing
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    catch (const po::error& e) {
        // print errors in command-line parser part
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    initCommon();
    dump();
}

Sequence::~Sequence()
{
    for (auto& pass : m_passes)
    {
        if (pass.fboId)
            glDeleteFramebuffers(1, &pass.fboId);
    }

    if (m_vaoId)
        glDeleteVertexArrays(1, &m_vaoId);
    if (m_vboId)
        glDeleteBuffers(1, &m_vboId);
    if (m_iboId)
        glDeleteBuffers(1, &m_iboId);
}

void Sequence::initCommon()
{
    //
    // Finish pass setup
    //

    //
    // Go through pass inputs, loading textures from disk, skipping references
    //

    for (auto& pass : m_passes)
    {
        for (auto& inputIt : pass.inputs)
        {
            auto& input = inputIt.second;

            // Load the texture file
            switch (input.uniform->type)
            {
            case GL_SAMPLER_2D:
            case GL_IMAGE_2D:
            {
                //if (input.path.empty())
                //    break;
                if (input.path.find("::") != std::string::npos)
                    break;

                // is the texture already loaded?
                auto textureIt = m_textures.find(input.path);

                if (textureIt == m_textures.end())
                {
                    // load texture from disk
                    int width, height, channels, alphaChannel;
                    OIIO::TypeDesc format;
                    void* data = nullptr;

                    if (!image_utils::load_image(input.path, input.attributes, width, height, data, channels, alphaChannel, format)) {
                        if (data)
                            free(data);

                        exit(1);
                    }

                    // Determine texture internal format
                    GLenum internalFormat, type;

                    const GLenum list[4][4] =
                    {
                        { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
                        { GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 },
                        { GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F },
                        { GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F },
                    };

                    switch (format.basetype)
                    {
                    case OIIO::TypeDesc::UINT8:
                        internalFormat = list[0][channels - 1];
                        type = GL_UNSIGNED_BYTE;
                        break;
                    case OIIO::TypeDesc::UINT16:
                        internalFormat = list[1][channels - 1];
                        type = GL_UNSIGNED_SHORT;
                        break;
                    case OIIO::TypeDesc::HALF:
                        internalFormat = list[2][channels - 1];
                        type = GL_HALF_FLOAT;
                        break;
                    case OIIO::TypeDesc::FLOAT:
                        internalFormat = list[3][channels - 1];
                        type = GL_FLOAT;
                        break;
                    default:
                        assert(0);
                        exit(1);
                        break;
                    }

                    textureIt = m_textures.insert({ input.path, std::make_shared<Texture>(width, height, internalFormat, type, data, alphaChannel) }).first;

                    if (data)
                        free(data);
                }

                input.texture = textureIt->second;
                break;
            }
            default:
                break;
            }
        }
    }

    //
    // Go through pass inputs/outputs, creating framebuffers & setting referenced output texture pointers
    //

    int passIndex = 0;

    for (auto& pass : m_passes)
    {
        //
        // Set viewport size
        //

        assert(!pass.sizeText[0].empty());

        for (int i = 0; i < 2; i++)
        {
            if (pass.sizeText[i].find("::") == std::string::npos)
            {
                // Numeric value
                pass.size[i] = str_to_numeric<int32_t>(pass.sizeText[i]);
                continue;
            }

            // Get size from referenced input texture
            const size_t split = pass.sizeText[i].find("::");
            const auto refInputName = pass.sizeText[i].substr(0, split);
            const int refPassIndex = str_to_numeric<int32_t>(pass.sizeText[i].substr(split + 2));

            if (refInputName.empty())
            {
                LOG(error) << "pass_size (" << passIndex << "): empty referenced input name.";
                exit(1);
            }

            if (refPassIndex < 0 || refPassIndex >= m_passes.size())
            {
                LOG(error) << "pass_size (" << passIndex << "): wrong referenced pass index " << refPassIndex;
                exit(1);
            }

            const auto& refPass = m_passes[refPassIndex];

            auto refInputIt = refPass.inputs.find(refInputName);

            if (refInputIt == refPass.inputs.end())
            {
                LOG(error) << "pass_size (" << passIndex << "): input " << refInputName << " not found in referenced pass " << refPassIndex;
                exit(1);
            }

            pass.size[i] = i == 0 ? refInputIt->second.texture->getWidth() : refInputIt->second.texture->getHeight();
        }

        if (pass.isCompute)
        {
            //LOG(debug) << "Work group size: " << pass.workGroupSizeText[0] << " " << pass.workGroupSizeText[1];

            // Set the number of threads
            for (int i = 0; i < 2; i++)
                pass.workGroupSize[i] = str_to_numeric<int32_t>(pass.workGroupSizeText[i]);
        }
        else
        {
            // Create offscreen buffer
            glGenFramebuffers(1, &pass.fboId);
            glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId);
        }

        //
        // Go through all outputs
        //

        for (auto& outputIt : pass.outputs)
        {
            auto& output = outputIt.second;

            if (output.alphaChannel >= output.channels)
            {
                LOG(error) << "out_alpha_channel (" << outputIt.first << "): index exceeds max channel index for this image.";
                exit(1);
            }

            if (!output.path.empty())
            {
                // Evaluate saved file format (int/float, bit depth)
                output.format = image_utils::get_output_format(output.path, output.bits, output.formatDefaulted);
            }

            //
            // Create a texture render target for this output
            //

            std::string textureName(outputIt.first + "::" + std::to_string(passIndex));

            // Get its format
            const std::pair<std::string, GLenum> formats[]
            {
                { "rgba8", GL_RGBA8 },
                { "rgba16", GL_RGBA16 },
                { "rgba16f", GL_RGBA16F },
                { "rgba32f", GL_RGBA32F },
                { "r8", GL_R8 },
                { "r16", GL_R16 },
                { "r16f", GL_R16F },
                { "r32f", GL_R32F },
                { "rg8", GL_RG8 },
                { "rg16", GL_RG16 },
                { "rg16f", GL_RG16F },
                { "rg32f", GL_RG32F },
                { "rgb8", GL_RGB8 },
                { "rgb16", GL_RGB16 },
                { "rgb16f", GL_RGB16F },
                { "rgb32f", GL_RGB32F }
            };

            GLenum internalFormat;
            int i;

            for (i = 0; i < 16; i++)
            {
                if (output.internalFormatText == formats[i].first)
                {
                    internalFormat = formats[i].second;
                    break;
                }
            }

            if (i == 16)
            {
                LOG(warning) << "Pass " << passIndex << ": unknown output framebuffer format " << output.internalFormatText << " changing to rgba32f.";

                // Set default
                internalFormat = GL_RGBA32F;
            }
            else
            {
                if (i > 3 && pass.isCompute)
                {
                    // Switch to the appropriate RGBA type
                    i %= 4;
                    LOG(warning) << "Pass " << passIndex << ": only 4-component output framebuffer formats are allowed for compute shaders, changing to " << formats[i].first;
                }

                // Got it
                internalFormat = formats[i].second;
            }

            // Create the texture
            auto textureIt = m_textures.insert({ textureName, std::make_shared<Texture>(pass.size[0], pass.size[1], internalFormat, GL_FLOAT, nullptr, output.alphaChannel) }).first;
            output.texture = textureIt->second;

            //LOG(debug) << "Pass output texture ID on init: " << output.texture->getId();

            if (!pass.isCompute)
            {
                // Attach it to the FBO
                LOG(debug) << "Pass " << passIndex << ": attaching output " << output.output->location << " " << outputIt.first << " to FBO";
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + output.output->location, GL_TEXTURE_2D, textureIt->second->getId(), 0);
            }
        }

        if (!pass.isCompute)
        {
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

            if (status != GL_FRAMEBUFFER_COMPLETE) {
                throw std::exception("Pass %i: Unable to setup FBO.", passIndex);
                exit(1);
            }

            LOG(debug) << "Pass " << passIndex << ": FBO created successfully.";
        }

        // Go through inputs to set referenced texture pointers
        for (auto& inputIt : pass.inputs)
        {
            auto& input = inputIt.second;

            // Load the actual texture
            switch (input.uniform->type)
            {
            case GL_SAMPLER_2D:
            case GL_IMAGE_2D:
            {
                if (input.path.find("::") == std::string::npos)
                //if (input.path.empty())
                    continue;

                // is the texture already loaded?
                auto textureIt = m_textures.find(input.path);

                if (textureIt == m_textures.end())
                {
                    // Not finding the texture means the referenced output is missing,
                    // which should never happen unless we made a mistake during parsing stage
                    LOG(error) << "input (" << inputIt.first << ": referenced output is missing texture.";
                    exit(1);
                }

                input.texture = textureIt->second;
                break;
            }
            default:
                break;
            }
        }

        passIndex++;
    }

    //
    // Create unit quad
    //

    const float vertices[] = {
        // position    // uv
        -1.0, -1.0,    0.0, 0.0,
        -1.0,  1.0,    0.0, 1.0,
        1.0,  1.0,    1.0, 1.0,
        1.0, -1.0,    1.0, 0.0
    };

    const unsigned int indices[] = {
        0, 2, 1,
        0, 3, 2
    };

    // Definte winding order and face culling
    GLCall(glFrontFace(GL_CCW));
    GLCall(glCullFace(GL_BACK));
    GLCall(glEnable(GL_CULL_FACE));

    GLCall(glGenVertexArrays(1, &m_vaoId));
    GLCall(glGenBuffers(1, &m_vboId));
    GLCall(glGenBuffers(1, &m_iboId));

    GLCall(glBindVertexArray(m_vaoId));

    GLCall(glBindBuffer(GL_ARRAY_BUFFER, m_vboId));
    GLCall(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

    GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_iboId));
    GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

    GLCall(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0));
    GLCall(glEnableVertexAttribArray(0));

    GLCall(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(2 * sizeof(float))));
    GLCall(glEnableVertexAttribArray(1));
}

void Sequence::run()
{
    Timer timer;

    LOG(debug) << "Rendering.";

    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

    for (auto& pass : m_passes)
    {
        glUseProgram(pass.program->getId());

        //
        // Send uniform data, bind textures etc.
        //

        int textureIndex = 0;

        for (auto& inputIt : pass.inputs)
        {
            auto& input = inputIt.second;

            switch (input.uniform->type)
            {
            case GL_SAMPLER_2D:
            case GL_IMAGE_2D:
            {
                GLuint textureId = input.texture->getId();

                if (pass.isCompute)
                {
                    //GLCall(glBindImageTexture(textureIndex, textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F));
                    //GLCall(glBindImageTexture(textureIndex, textureId, 0, GL_FALSE, 0, GL_READ_ONLY, input.texture->getInternalFormat()));
                    glActiveTexture(GL_TEXTURE0 + textureIndex);
                    glBindTexture(GL_TEXTURE_2D, textureId);

                    LOG(debug) << "Texture " << textureId << " binding is " << textureIndex;
                }
                else
                {
                    glActiveTexture(GL_TEXTURE0 + textureIndex);
                    glBindTexture(GL_TEXTURE_2D, textureId);
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, input.tex_min);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, input.tex_mag);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, input.tex_s);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, input.tex_t);

                if (input.tex_min != GL_LINEAR && input.tex_min != GL_NEAREST) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0);
                    glGenerateTextureMipmap(textureId); // NOTE: OpenGL 4.5+
                    LOG(debug) << "Generated mip-maps for " << inputIt.first << " at " << inputIt.second.texture;
                }

                input.uniform->set(textureIndex++);
                //glUniform1i(glGetUniformLocation(pass.program->getId(), "u_texture0"), textureIndex++);
                break;
            }
            case GL_BOOL:
            case GL_BOOL_VEC2:
            case GL_BOOL_VEC3:
            case GL_BOOL_VEC4:
            case GL_INT:
            case GL_INT_VEC2:
            case GL_INT_VEC3:
            case GL_INT_VEC4:
                input.uniform->set(&input.ints[0]);
                break;
            default:
                input.uniform->set(&input.floats[0]);
                break;
            }
        }

        //
        // Render/compute
        //

        if (pass.isCompute)
        {
            // Bind output images
            for (auto& outputIt : pass.outputs)
            {
                auto& output = outputIt.second;

                //glActiveTexture(GL_TEXTURE0 + textureIndex);
                //glBindTexture(GL_TEXTURE_2D, output.texture->getId());
                GLCall(glBindImageTexture(textureIndex, output.texture->getId(), 0, GL_FALSE, 0, GL_WRITE_ONLY, output.texture->getInternalFormat()));

                LOG(debug) << "Texture " << output.texture->getId() << " binding is " << textureIndex;

                output.uniform->set(textureIndex++);
            }

            // Distribute the work groups across the number of threads
            GLCall(glDispatchCompute(
                (pass.size[0] + pass.workGroupSize[0] - 1) / pass.workGroupSize[0],
                (pass.size[1] + pass.workGroupSize[1] - 1) / pass.workGroupSize[1],
                1));
            GLCall(glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT));
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId);

            std::vector<GLenum> buffers(8, GL_NONE);

            for (auto& outputIt : pass.outputs)
            {
                auto& output = outputIt.second;
                buffers[output.output->location] = GL_COLOR_ATTACHMENT0 + output.output->location;
            }

            //for (int i = 0; i < 8; i++)
                //buffers.push_back(GL_COLOR_ATTACHMENT0 + i);

            //glReadBuffer(buffers[0]);
            glDrawBuffers((GLsizei)buffers.size(), &buffers[0]);

            glViewport(0, 0, pass.size[0], pass.size[1]);

            glClearColor(0.f, 0.f, 0.f, 0.f);
            glClear(GL_COLOR_BUFFER_BIT);

            glBindVertexArray(m_vaoId);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        glFinish();

        //
        // Write output files, if any
        //

        for (auto& outputIt : pass.outputs)
        {
            auto& output = outputIt.second;
            output.saveTexture();
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    LOG(debug) << "Sequence completed in " << timer.nowText();
}

#define STRING_USED_DEFAULTS "(used default)"
#define STRING_CHANGED_TO_SUPPORTED "(changed to highest supported value for file format)"

void Sequence::dump()
{
#if 0
    LOG(info) <<
        // --- Format string ---
        "Options:\n"
        // Input files.
        //"input_file = \"%s\"\n"
        // glsl file.
        //"glsl_file = \"%s\"\n"
        // lut file.
        //"lut_file = \"%s\"\n"
        // Output file.
        //"output_file = \"%s\"\n\n"
        // Processing options.
        "channels = %i %s\n\n"
        "outchannels = %i %s\n\n"
        // RAW Processing options.
        "color space = %s\n"
        "demozaic = %s\n\n"

        "positive = %s %s\n\n"
        // Write options.
        "Write options:\n"
        "output_bit_depth = %ibit %s%s\n"
        "output_compression = %s\n"
        "\n",
        // --- Format variadic arguments ---

        // TODO: Display pass info.

        // Processing options.
        channels,
        vm["channels"].defaulted() ? str_used_defaults : "",
        outchannels,
        vm["outchannels"].defaulted() ? str_used_defaults : "",
        // RAW Processing options.
        vm["colors"].defaulted() ? str_used_defaults : "sRGB",
        vm["demosaic"].defaulted() ? str_used_defaults : "AHD",
        positive ? "true" : "false",
        vm["positive"].defaulted() ? str_used_defaults : "",
        // Write options.
        arg_output_format,
        vm["output_bit_depth"].defaulted() ? str_used_defaults : "",
        output_format_defaulted ? str_changed_to_supported : "",
        output_compression.empty() ? " - " : output_compression.c_str()
        );
#endif
    }
