/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by   //-V1042
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

 // This is an open source non-commercial project. Dear PVS-Studio, please check it.
 // PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "Sequence.h"

#include "OpenGLUtils.h"
#include "Timer.h"
#include "Log.h"
#include "GLProgramManager.h"
#include "ImageUtils.h"

#include <boost/program_options.hpp>
#include <boost/program_options/option.hpp>
#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/program_options/value_semantic.hpp>

#include <termcolor/termcolor.hpp>

#include "rapidobj/rapidobj.hpp"
#include "mesh_io.h"

namespace po = boost::program_options;
namespace ro = rapidobj;

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
            ("bg_color", po::value<std::vector<std::string>>()->multitoken()->default_value(std::vector<std::string>{ "0.0", "0.0", "0.0", "1.0" }),
                "Background color (RGBA):\n"
                " --bg_color R G B A\n"
                )
			("cull", po::value<std::vector<std::string>>()->multitoken(),
                (
				    "Culling mode:\n"
                    + Pass::get_possible_culling_fmt() +
					"Supported culling modes:\n"
					" GL_CW, GL_CCW, GL_FRONT, GL_BACK, true, false"
					" (default: GL_CW, GL_BACK, true)"
					" --cull wind cw face bk enable true"
					).c_str()
				)
            ("pass_mesh,M", po::value<std::vector<std::string>>()->multitoken(),
				(
                    "3D mesh (*.PLY) to use for this pass:\n"
                    "no pass_mesh directive = render using quad or\n"
                    "--pass_mesh quad - render using quad\n"
                    "loading mesh from file:\n"
                    "--pass_mesh mesh::0 tris true rndr poly path¥to¥mesh.exe\n"
                    + MeshInput::get_possible_mesh_parm_fmt() +
                    "Supported mesh attributes:\n"
                    " Vertex coodinates: vec3 pos\n"
                    " Vertex normals: vec3 normals\n"
                    " UVs: vec2 uvs\n"
                    " Vertex colors: vec4 colors\n"
                    ).c_str()
                )
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
			("atomic,B", po::value<std::vector<std::string>>()->multitoken(),
                "Atomic counter\n"
				"Counter/buffer uniform name, binding, [size], [initial value]")
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
				"JPEG2000: *.jp2, *.j2k\n"
				"WEBP:     *.webp\n"
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
				"JPEG2000: 8, 16\n"
				"WEBP:     8\n"
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
            std::cout << termcolor::bright_yellow << termcolor::bold;
			std::cout << APP_NAME << " version " << APP_VERSION[0] << "." << APP_VERSION[1] << "." << APP_VERSION[2] << " Copyright (c) " << APP_AUTHOR << std::endl;
            std::cout << "Build from: " << __DATE__ << ", " << __TIME__ << "." << std::endl;
            // Get available GPU features
            get_GPUfeatures();

            infoExit = true;
        }

        if (infoExit)
            exit(1);

        // set the logger verbosity
		Log_SetVerbosity(std::clamp(vm["verbosity"].as<int>(), 0, 5));

        //
        // parse the multi-occurence, multi-token options here
        //
        LOG(debug) << "Starting RawGL sequence" << std::endl;

        Pass* currentPass = nullptr;
        PassInput* currentInput = nullptr;
        PassOutput* currentOutput = nullptr;
        MeshInput* currentMeshInput = nullptr;
        PassInputCounters* currentInputCounters = nullptr;
        size_t currentPassN = 0;

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
				// TODO:
                // mtp/mp/scl/sc option for size scale vec2() positive only
				// mg/mrgn option for image margin vec4() (left, right, top, bottom) positive or negative
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
            // background color
            // 
            if (o.string_key == "bg_color")
            {
                if (!currentPass)
                {
                    LOG(error) << "bg_color: no preceeding pass declaration.";
                    exit(1);
                }
                int size = o.value.size();
                if (size < 1 || size > 4)
                {
					LOG(error) << "bg_color: must have at least 1 parameter.";
					exit(1);
				}
                // set clearColor
                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(size);
                std::copy(o.value.begin(), o.value.end(), std::back_inserter(val_arr));

                GLfloat tmp_floats[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

                for (uint8_t i = 0; i < size; ++i) {
                    const std::string& str_val = val_arr[i];
                    tmp_floats[i] = str_to_numeric<float_t>(hr, str_val);
                }

                if (hres::OK == hr) {
                    memcpy(&currentPass->clearColor, &tmp_floats, sizeof(GLfloat) * 4);
                }
                LOG(debug) << "Clear color (bg_color) set as RGBA [" << std::fixed << std::setprecision(4) <<
                    tmp_floats[0] << ", " << tmp_floats[1] << ", " << tmp_floats[2] << ", " << tmp_floats[3] << "]";
			}
            //
            // mesh parsing
            // 
            if (o.string_key == "pass_mesh")
            {
                if (!currentPass)
                {
                    LOG(error) << "pass_mesh: no preceeding pass declaration.";
                    exit(1);
                }

                if (o.value.size() < 1)
                {
                    LOG(error) << "pass_mesh: must have at least 1 parameter.";
                    exit(1);
                }

                auto val_key = o.value[0];
                auto type_name = o.value[0];

                const size_t split = o.value[0].find("::");
                type_name = o.value[0].substr(0, split);
                
                auto [meshesIt, success] = currentPass->meshes.insert({ o.value[0], MeshInput() });
                currentMeshInput = &meshesIt->second;

                if (type_name == "mesh")
                {

                    currentMeshInput->mesh.isQuad = false;

                    if (!success)
                    {
                        LOG(error) << "mesh (" << o.value[0] << "): duplicate id.";
                        exit(1);
                    }

                    // Not work at this moment: Mesh_name::Pass# 
                    // in a future for support variable meshes per passess
                    if (split != std::string::npos) {
                        hres hr_convert = hres::OK;
                        const int ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_key.substr(split + 2));

                        if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                            auto& ref_pass = m_passes[ref_pass_index];

                            auto ref_input_it = ref_pass.inputs.find(type_name);
                            if (ref_input_it != ref_pass.inputs.end()) {
                                auto& ref_input = ref_input_it->second;
                            }
                            else {
                                LOG(error) << "pass_mesh: reference input not found.";
                                exit(1);
                            }
                        };
                    };

                    hres hr = hres::OK;
                    std::vector<std::string> val_arr;
                    val_arr.reserve(o.value.size() - 1);
                    std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                    
                    std::string mesh_path = val_arr[0];
                    std::string file_ext = get_file_ext(mesh_path);
                    if (file_ext != "ply" && file_ext != "obj") {
                        LOG(error) << "Only PLY and OBJ meshes supported.";
                        exit(1);
                    }
                    currentMeshInput->mesh.isQuad = false;
                    currentMeshInput->mesh.FileName = mesh_path;

                    for (size_t i = 1; i < val_arr.size() - 1; ++i) {
                        val_key = val_arr[i];
                        std::string val_data;

                        val_data = val_arr[i + 1];

                        // Search for texture attributes
                        hres hr_tex_attr = hres::OK;
                        currentMeshInput->eval_mesh_parm(hr_tex_attr, val_key, val_data);

                        if (hr_tex_attr != hres::OK) {
							LOG(error) << "pass_mesh: unknown mesh parameter.";
							exit(1);
						}
                        i++;
                    }
                    
                }
                else if (type_name == "quad") {
                    currentMeshInput->mesh.isQuad = true;
                    LOG(info) << "pass_mesh: Default Quad";
                }
                else {
                    LOG(error) << "pass_mesh: unknown mesh type.";
                    exit(1);
                }
            };

            if (o.string_key == "cull")
            {
                if (!currentPass)
                {
                    LOG(error) << "cull: no preceeding pass declaration";
                    exit(1);
                }

                if (o.value.size() < 1)
                {
                    LOG(error) << "cull: must have at least 1 parameter.";
                    exit(1);
                }

                auto val_key = o.value[0];

                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(o.value.size() - 1);
                std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                for (size_t i = 1; i < val_arr.size() - 1; ++i) {
                    val_key = val_arr[i];
                    std::string val_data;

                    val_data = val_arr[i + 1];

                    // Search for texture attributes
                    hres hr_tex_attr = hres::OK;
                    currentPass->eval_cull_parm(hr_tex_attr, val_key, val_data);

                    if (hr_tex_attr != hres::OK) {
                        LOG(error) << "cull: unknown mesh parameter.";
                        exit(1);
                    }
                    i++;
                }
            };

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

                        /////////////
                        if (val_key.find("::") != std::string::npos) {
                            hres hr_convert = hres::OK;
                            const size_t split = val_key.find("::");
                            const auto val_name = val_key.substr(0, split);
                            const int ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_key.substr(split + 2));

                            if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                                auto& ref_pass = m_passes[ref_pass_index];
								// looks into Outputs list ins ref pass
                                auto ref_output_it = ref_pass.outputs.find(val_name);
                                
                                if (ref_output_it == ref_pass.outputs.end()) {
									// if output not found in Ouptut list,
									// that can be a output defined in a shader but not used as a cli output
                                    // create a new Output in that Shader Pass 
                                    ref_output_it = ref_pass.outputs.insert({ val_name, PassOutput() }).first;
                                }

                                auto& ref_output = ref_output_it->second;

                                hres hr_find_uniform = hres::OK;

                                if (ref_pass.isCompute) {
                                    ref_output.uniform = ref_pass.program->findUniform(val_name);

                                    if (!ref_output.output) {
                                        hr_find_uniform = hres::ERR;
                                    }
                                }
                                else {
                                    ref_output.output = ref_pass.program->findOutput(val_name);

                                    if (!ref_output.output) {
                                        hr_find_uniform = hres::ERR;
                                    }
                                }

                                if (hres::OK == hr_find_uniform) {
                                    i++; // Jump over next iteration, it contains only value which was already read
                                }
                                else {
                                    LOG(error) << "in (" << o.value[0] << "): referenced program output " << val_key << " not found.";
                                    exit(1);
                                }
                            }
                            currentInput->path = val_key;
                            i++;
                            continue;
                        }
						
                        if (i == val_arr.size() - 1) {// if only Texture name and Path
                            currentInput->path = val_key;
                            break;
                        }
						
                        val_data = val_arr[i + 1];
                        // Search for texture attributes
                        hres hr_tex_attr = hres::OK;
                        currentInput->eval_tex_attr(hr_tex_attr, val_key, val_data);

                        if (hres::OK == hr_tex_attr) {
                            i++;
                        }
                        else {
                            //LOG(error) << "in (" << o.value[0] << "): " << val_key << "invalid ( " << val_data << " ) value";
                            currentInput->path = val_key;
                        }
                        //else {
                        //    // Search for value from previous passes.
                        //    hres hr_convert = hres::OK;
                        //    const int32_t ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_data);

                        //    if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                        //        auto& ref_pass = m_passes[ref_pass_index];
                        //        auto ref_output_it = ref_pass.outputs.find(val_key);

                        //        if (ref_output_it == ref_pass.outputs.end()) {
                        //            ref_output_it = ref_pass.outputs.insert({ val_key, PassOutput() }).first;
                        //        }

                        //        auto& ref_output = ref_output_it->second;

                        //        hres hr_find_uniform = hres::OK;

                        //        if (ref_pass.isCompute) {
                        //            ref_output.uniform = ref_pass.program->findUniform(val_key);

                        //            if (!ref_output.output) {
                        //                hr_find_uniform = hres::ERR;
                        //            }
                        //        }
                        //        else {
                        //            ref_output.output = ref_pass.program->findOutput(val_key);

                        //            if (!ref_output.output) {
                        //                hr_find_uniform = hres::ERR;
                        //            }
                        //        }

                        //        if (hres::OK == hr_find_uniform) {
                        //            i++; // Jump over next iteration, it contains only value which was already read
                        //        }
                        //        else {
                        //            LOG(error) << "in (" << o.value[0] << "): referenced program output " << val_data.c_str() << " not found.";
                        //            exit(1);
                        //        }
                        //        //hacky fix for previous pass output 
                        //        currentInput->path = (val_key + "::" + val_data);
                        //    }
                        //    else {
                        //        // no usable hr_attrib was found
                        //        currentInput->path = val_key;
                        //    }
                        //}
                    }
                }
                else { // Numeric values
                    uint8_t num_fields = 1;
                    bool is_floats = false;
                    bool is_double = false;

                    switch (currentInput->uniform->type) {
                    case GL_BOOL:
                    case GL_INT:
                        num_fields = 1;
                        break;
                    case GL_BOOL_VEC2:
                    case GL_INT_VEC2:
                        num_fields = 2;
                        break;
                    case GL_BOOL_VEC3:
                    case GL_INT_VEC3:
                        num_fields = 3;
                        break;
                    case GL_BOOL_VEC4:
                    case GL_INT_VEC4:
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
                    case GL_DOUBLE:
						is_double = true;
						num_fields = 1;
						break;
                    case GL_DOUBLE_VEC2:
                        is_double = true;
                        num_fields = 2;
                        break;
                    case GL_DOUBLE_VEC3:
                        is_double = true;
						num_fields = 3;
						break;
                    case GL_DOUBLE_VEC4:
						is_double = true;
                        num_fields = 4;
                        break;
                    case GL_DOUBLE_MAT2:
                        is_double = true;
						num_fields = 4;
						break;
                    case GL_DOUBLE_MAT2x3:
                    case GL_DOUBLE_MAT3x2:
						is_double = true;
                        num_fields = 6;
						break;
                    case GL_DOUBLE_MAT2x4:
                    case GL_DOUBLE_MAT4x2:
                        is_double = true;
						num_fields = 8;
                        break;
                    case GL_DOUBLE_MAT3:
                        is_double = true;
                        num_fields = 9;
                        break;
                    case GL_DOUBLE_MAT3x4:
                    case GL_DOUBLE_MAT4x3:
						is_double = true;
						num_fields = 12;
						break;
                    case GL_DOUBLE_MAT4:
						is_double = true;
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
                    GLdouble tmp_doubles[PassInput::NUM_DOUBLES] = { 0.0 };

                    for (uint8_t i = 0; i < num_fields; ++i) {
                        const std::string& str_val = val_arr[i];

                        if (is_floats) {
                            tmp_floats[i] = str_to_numeric<float_t>(hr, str_val);
                        }
						else if (is_double) {
							tmp_doubles[i] = str_to_numeric<double_t>(hr, str_val);
						}
                        else {
                            tmp_ints[i] = str_to_numeric<int32_t>(hr, str_val);
                        }
                    }

                    if (hres::OK == hr) {
                        if (is_floats) {
                            memcpy(&currentInput->floats, &tmp_floats, sizeof(GLfloat) * PassInput::NUM_FLOATS);
                        }
                        else if (is_double) {
                            memcpy(&currentInput->doubles, &tmp_doubles, sizeof(GLdouble) * PassInput::NUM_DOUBLES);
						}
                        else {
                            memcpy(&currentInput->ints, &tmp_ints, sizeof(GLint) * PassInput::NUM_INTS);
                        }
                    }
                }

            }
            else if (o.string_key == "atomic")
            {
                if (!currentPass)
                {
                    LOG(error) << "atomic (" << o.value[0] << "): no preceeding input declaration.";
                    exit(1);
                }
                if (o.value.size() < 2)
                {
                    LOG(error) << "atomic (" << o.value[0] << "): must have at least 2 parameters.";
                    exit(1);
                }

                if (o.value[0] == "cntr") {
                    hres hr = hres::OK;
                    std::vector<std::string> val_arr;
                    val_arr.reserve(o.value.size() - 1);
                    std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                    GLuint tmp_uint = 0;
                    
                    if (val_arr.size() > 2) {
                        LOG(error) << "atomic (" << o.value[0] << "): can only have a single value";
                    }
                    else {
						tmp_uint = static_cast<GLuint>(str_to_numeric<uint32_t>(hr, val_arr[1]));
					}
                    if (hr == hres::ERR) {
                        LOG(error) << "atomic (" << o.value[0] << "): -> " << o.value[1] << " <- is invalid parameters";
                        exit(1);
                    }

                    auto counterSh = currentPass->program->findCounter(val_arr[0]);
                    if (!counterSh) {
						LOG(error) << "atomic (" << o.value[0] << "): referenced counter " << val_arr[0].c_str() << " not found.";
						exit(1);
					}

                    auto [i_counterIt, success] = currentPass->inputCounters.insert({ val_arr[0], Pass::inputCounter() });
                    //auto [counterIt, success] = m_aCounters.insert({ val_arr[0], PassInputCounters() });
                    i_counterIt->second.size = counterSh->size;
                    i_counterIt->second.value = { tmp_uint };

                    LOG(debug) << "atomic counter: " << val_arr[0].c_str() << " set to " << tmp_uint;

                    if (!success) {
                        LOG(error) << "atomic counter: " << val_arr[0].c_str() << " already exists.";
                        exit(1);
                    }
                }
                else if (o.value[0] == "buff") {
                    hres hr = hres::OK;
                    std::vector<std::string> val_arr;
                    val_arr.reserve(o.value.size() - 1);
                    std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));
                }
                else {
                    LOG(error) << "atomic (" << o.value[0] << "): unknown atomic buffer type";
					//exit(1);
                };
                // link the input to its atomic counters
                //auto [inputIt, success] = currentPass->u_aBuffers.insert({ o.value[0], PassInputCounters() });
/*
                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(o.value.size() - 1);
                std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));
				
                int cntr_binding = 0;
                int cntr_offset = 0;
                GLuint cntr_val = 0;

                for (size_t i = 0; i < val_arr.size(); ++i) {
                    std::string val_key = val_arr[i];
                    std::string val_data;

                    if (i < val_arr.size() - 1) {
                        val_data = val_arr[i + 1];
                    }

                    // Search for texture attributes
                    hres hr_tex_parm = hres::OK;
                    currentInputCounters->eval_counter_parm(hr_tex_parm, val_key);

                    if (hres::OK != hr_tex_parm) {
                        LOG(error) << "atomic (" << o.value[0] << "): -> " << val_key << " <- is invalid parameters";
                        exit(1);
                    }
                    int tmp_int = str_to_numeric<int32_t>(hr, val_data);
                    
                    //if (hr == hres::ERR) {
                    //    LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- is invalid parameters";
                    //    exit(1);
                    //}

                    if (val_key == "vl") {
						
                        if (hr == hres::ERR && val_data.find("::") != std::string::npos)
                        {
                            const size_t split = val_data.find("::");
                            const auto refInputAtomicName = val_data.substr(0, split);
                            const int refPassIndex = str_to_numeric<int32_t>(val_data.substr(split + 2));
                            auto refPass = m_passes[refPassIndex].u_aCounters.find(refInputAtomicName);
							
						    if (refPass == m_passes[refPassIndex].u_aCounters.end()) {
                                LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- reference counter not found";
                                exit(1);
                            }
                            if (currentPassN <= refPassIndex) {
                                LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- reffering to counter from same or future pass";
                                exit(1);
                            }
                            inputIt->second.path = val_data;
                            //std::cout << "Atomic Name :" << refInputAtomicName << " from pass: " << refPassIndex;
                            break;
                        }
                        cntr_val = tmp_int;
                    }
                    else if (hr == hres::ERR) {
                        LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- is invalid parameters";
                           exit(1);
                    }
					
                    i++;
                }
                // get atomic counter reference from Atomic Buffers list by defined binding
                //auto shaderAtomics = currentPass->program->findBuffer(o.value[1]);

                int counterBinding = cntr_binding + cntr_offset / sizeof(GLuint);
                std::string name = "test";
                auto passCounter = currentPass->program->findCounter(name);

                if (!passCounter)
                {
                    LOG(error) << "atomic (" << o.value[0] << " (binding = " << cntr_binding 
                        << " ,offset = " << cntr_offset << " ): program atomic counter not found.";
                    continue;
                    //exit(1);
                }
                inputIt->second.binding = cntr_binding;
                inputIt->second.offset = cntr_offset;
                if (inputIt->second.path == "") {
                    inputIt->second.value = cntr_val;
                }

                switch ( val_arr.size() )
                {
				case 2:
                    LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << val_arr[1] << " ), initial value = " << 0;
                    break;
                case 4:
                    LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << val_arr[1] << " ), initial value = " << val_arr[3];
                    break;
                case 6:
                    LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << val_arr[1] << " , offset = " << val_arr[3] << " ), initial value = " << val_arr[5];
                    break;
                default:
                    break;
                }
				
				//LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << o.value[1] << " ), initial value = " << o.value[2];
*/
				//currentInput->counter = o.value[1];
                LOG(trace) << "Test output only.";
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
    {
        int i = 0;
        for (auto& pass : m_passes) {
            size_t p = pass.u_aCounters.size();
            size_t a = pass.program->BuffersSize();
            if (p < a) {
                LOG(debug) << "Pass #" << i << ": " << p << " from " << a << " atomic counters are initialized";
                i++;
            }
        }
    };    
    initCommon();
}

Sequence::~Sequence()
{
    for (auto& pass : m_passes)
    {
        if (pass.fboId)
            glDeleteFramebuffers(1, &pass.fboId);
        if (pass.meshes.begin()->second.VBO.vaoId)
			glDeleteVertexArrays(1, &pass.meshes.begin()->second.VBO.vaoId);
        if (pass.meshes.begin()->second.VBO.vboId)
            glDeleteBuffers(1, &pass.meshes.begin()->second.VBO.vboId);
        if (pass.meshes.begin()->second.VBO.nboId)
            glDeleteBuffers(1, &pass.meshes.begin()->second.VBO.nboId);
        if (pass.meshes.begin()->second.VBO.tboId)
            glDeleteBuffers(1, &pass.meshes.begin()->second.VBO.tboId);
        if (pass.meshes.begin()->second.VBO.cboId)
            glDeleteBuffers(1, &pass.meshes.begin()->second.VBO.cboId);
        if (pass.meshes.begin()->second.VBO.iboId)
            glDeleteBuffers(1, &pass.meshes.begin()->second.VBO.iboId);
    }
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
                // To Delete 
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

                    const GLenum list[5][4] =
                    {
                        { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
                        { GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 },
                        { GL_R32UI,GL_RG32UI,GL_RGB32UI,GL_RGBA32UI},
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
                    case OIIO::TypeDesc::UINT32:
                        internalFormat = list[2][channels - 1];
                        type = GL_UNSIGNED_INT;
                        break;
                    case OIIO::TypeDesc::HALF:
                        internalFormat = list[3][channels - 1];
                        type = GL_HALF_FLOAT;
                        break;
                    case OIIO::TypeDesc::FLOAT:
                        internalFormat = list[4][channels - 1];
                        type = GL_FLOAT;
                        break;
                    default:
                        assert(0);
                        exit(1);
                        break;
                    }

#if 0   			// write data to binary file for debugging
                    FILE* fp = std::fopen("d:/oiio_read_debug.raw", "wb");
                    std::fwrite(data, width * height * channels * format.size(), 1, fp);
					std::fclose(fp);
#endif

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
        LOG(debug) << "Pass " << passIndex << ": pass_size is " << pass.size[0] << " x " << pass.size[1];
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
            GLCall(glGenFramebuffers(1, &pass.fboId));
            GLCall(glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId));
m_passes[passIndex].glbObject.FBO.push_back(Pass::FBOobject{ pass.fboId });
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
                GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + output.output->location, GL_TEXTURE_2D, textureIt->second->getId(), 0));
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
// Create mesh
//
    // Default unit quad arrays
    // Warning! New version use vec3 pos, old was vec2
    float def_verts[] = {
        -1.0f,-1.0f, 0.0f,   -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f, 0.0f,    1.0f, -1.0f, 0.0f
    };
    float def_texCrds[] = {
        0.f, 0.f,       0.f, 1.f,
        1.f, 1.f,       1.f, 0.f
    };
    float def_vNormals[] = {
        0.f, 0.f, 1.f,  0.f, 0.f, 1.f,
        0.f, 0.f, 1.f,  0.f, 0.f, 1.f
    };

    unsigned char def_vColors[] = {
        255, 255, 255, 255,  255, 255, 255, 255,
        255, 255, 255, 255,  255, 255, 255, 255
    };
    // CW winding order
    // for CCW use: 0, 2, 1, 0, 3, 2
    // for CW use: 0, 1, 2, 0, 2, 3
    unsigned int def_indxs[] = {
        0, 1, 2,        0, 2, 3
    };

    // for this moment mesh is the same in all passes
    Pass* defPass = &m_passes[0];
    MeshInput* cMesh = &defPass->meshes.begin()->second;
    MeshInput::Mesh& crntMesh = cMesh->mesh;

    crntMesh.pVerts = def_verts;
    crntMesh.pTexts = def_texCrds;
    crntMesh.pNorms = def_vNormals;
    crntMesh.pColrs = def_vColors;
    crntMesh.pIndxs = def_indxs;

    // size of default arrays
    crntMesh.vrtSize = static_cast<GLsizei>(sizeof(def_verts));
    crntMesh.texSize = static_cast<GLsizei>(sizeof(def_texCrds));
    crntMesh.nrmSize = static_cast<GLsizei>(sizeof(def_vNormals));
    crntMesh.clrSize = static_cast<GLsizei>(sizeof(def_vColors));
    crntMesh.idxSize = static_cast<GLsizei>(sizeof(def_indxs));

    crntMesh.numIndxs = static_cast<GLsizei>(sizeof(def_indxs) / sizeof(unsigned int));

    // if not a quad
    // load PLY mesh from file using miniply library
    if (!crntMesh.isQuad)
    {
        Timer timer;

        const int kFilenameBufferLen = 16 * 1024 - 1;
        char* filenameBuffer = new char[kFilenameBufferLen + 1];
        filenameBuffer[kFilenameBufferLen] = '\0';

        LOG(debug) << "Loading mesh...";

        TriMesh* trimesh = parse_file_with_miniply(crntMesh.FileName.c_str(), crntMesh.Triangles);
        bool ok = trimesh != nullptr;

        if (ok)
        {
            // get the data
            crntMesh.pVerts = trimesh->pos;
            crntMesh.pTexts = trimesh->uv;
            crntMesh.pNorms = trimesh->normal;
            crntMesh.pColrs = trimesh->color;
            crntMesh.pIndxs = trimesh->indices;

            // size of the data
            crntMesh.vrtSize = static_cast<GLsizei>(trimesh->numVerts * 3 * sizeof(float));
            crntMesh.texSize = static_cast<GLsizei>(trimesh->numVerts * 2 * sizeof(float));
            crntMesh.nrmSize = static_cast<GLsizei>(trimesh->numVerts * 3 * sizeof(float));
            crntMesh.clrSize = static_cast<GLsizei>(trimesh->numVerts * 4 * sizeof(unsigned char));
            crntMesh.idxSize = static_cast<GLsizei>(trimesh->numIndices * sizeof(unsigned int));
            // number of indices
            crntMesh.numIndxs = static_cast<GLsizei>(trimesh->numIndices);
        }
        else
        {
            LOG(error) << "Failed to load mesh: " << crntMesh.FileName << std::endl;
            exit(-1);
        }
        LOG(debug) << "Mesh loading completed in " << timer.nowText();

		delete[] filenameBuffer;
    }
        // end of PLY read
    /*
    ro::Result result = ro::ParseFile("w:/cube.obj");
    if (result.error)
	{
		LOG(error) << "Failed to parse file: " << result.error.code.message();
		exit(-1);
	}
    bool success = ro::Triangulate(result);

    if (success)
	{
		// get the data
		ptr_Verts = result.attributes.positions.data();
        ptr_Texts = result.attributes.texcoords.data();
        ptr_Norms = result.attributes.normals.data();
        ptr_Colrs = result.attributes.colors.data();
        //ptr_Indxs = result.shapes.back().mesh.indices.data();
		// size of the data
		m_vrtSize = static_cast<GLsizei>(result.attributes.positions.size() * sizeof(float));
		m_texSize = static_cast<GLsizei>(result.attributes.texcoords.size() * sizeof(float));
		m_nrmSize = static_cast<GLsizei>(result.attributes.normals.size() * sizeof(float));
		m_clrSize = static_cast<GLsizei>(result.attributes.colors.size() * sizeof(float));
		//m_idxSize = static_cast<GLsizei>(result.m_indices.size() * sizeof(unsigned int));
	}

    */

    // Define winding order and face culling
    // Use non standard OpenGL winding order
    // to match 3D models order.
    GLint wind_order = m_passes[0].cullMode.windOrder;
	GLint cull_face = m_passes[0].cullMode.cullFace;

    GLCall(glFrontFace(wind_order));
    GLCall(glCullFace(cull_face));

    if (m_passes[0].cullMode.cullFaceEnable) {
        GLCall(glEnable(GL_CULL_FACE));
    }
    else {
		GLCall(glDisable(GL_CULL_FACE));
    }
    
    // Enable polygon smooth
    //GLCall(glEnable(GL_POLYGON_SMOOTH));

    // Enable multisample anti-aliasing
    //GLCall(glEnable(GL_MULTISAMPLE));
    // Set the sample coverage value to 1.0
    //GLCall(glSampleCoverage(1.0, GL_FALSE));
    // Set the sample mask to enable all 16 samples
    //GLCall(glSampleMaski(0, 0xFFFF));

    // for this moment mesh is the same in all passes
    defPass = &m_passes[0];
    cMesh = &defPass->meshes.begin()->second;
    crntMesh = cMesh->mesh;
    MeshInput::VertexBuffer& crntVBO = cMesh->VBO;
    
    //crntMesh = crntPass.mesh;

    //GLuint* m_vaoId, m_vboId, m_tboId, m_nboId, m_cboId, m_iboId;
    auto& m_vaoId = crntVBO.vaoId;
    auto& m_vboId = crntVBO.vboId;
    auto& m_iboId = crntVBO.iboId;
    auto& m_tboId = crntVBO.tboId;
    auto& m_nboId = crntVBO.nboId;
    auto& m_cboId = crntVBO.cboId;

    GLCall(glCreateVertexArrays(1, &m_vaoId));
    GLCall(glGenBuffers(1, &m_vboId));
    GLCall(glGenBuffers(1, &m_iboId));

    GLCall(glBindVertexArray(m_vaoId));
    
    // Generate and bind position buffer
    GLCall(glCreateBuffers(1, &m_vboId));
    GLCall(glNamedBufferData(m_vboId, crntMesh.vrtSize, static_cast<const void*>(crntMesh.pVerts), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(m_vaoId, 0, m_vboId, 0, 3 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(m_vaoId, 0, 3, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(m_vaoId, 0, 0));
    GLCall(glEnableVertexArrayAttrib(m_vaoId, 0));

    // Generate and bind texture coordinate buffer
    GLCall(glCreateBuffers(1, &m_tboId));
    GLCall(glNamedBufferData(m_tboId, crntMesh.texSize, static_cast<const void*>(crntMesh.pTexts), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(m_vaoId, 1, m_tboId, 0, 2 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(m_vaoId, 1, 2, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(m_vaoId, 1, 1));
    GLCall(glEnableVertexArrayAttrib(m_vaoId, 1));

    // Generate and bind normals buffer
    GLCall(glCreateBuffers(1, &m_nboId));
    GLCall(glNamedBufferData(m_nboId, crntMesh.nrmSize, static_cast<const void*>(crntMesh.pNorms), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(m_vaoId, 2, m_nboId, 0, 3 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(m_vaoId, 2, 3, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(m_vaoId, 2, 2));
    GLCall(glEnableVertexArrayAttrib(m_vaoId, 2));

    // Generate and bind color buffer
    GLCall(glCreateBuffers(1, &m_cboId));
    GLCall(glNamedBufferData(m_cboId, crntMesh.clrSize, static_cast<const void*>(crntMesh.pColrs), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(m_vaoId, 3, m_cboId, 0, 4 * sizeof(unsigned char)));
    GLCall(glVertexArrayAttribIFormat(m_vaoId, 3, 4, GL_UNSIGNED_BYTE, 0));
    GLCall(glVertexArrayAttribBinding(m_vaoId, 3, 3));
    GLCall(glEnableVertexArrayAttrib(m_vaoId, 3));

    // Generate and bind index buffer
    GLCall(glCreateBuffers(1, &m_iboId));
    GLCall(glNamedBufferData(m_iboId, crntMesh.idxSize, static_cast<const void*>(crntMesh.pIndxs), GL_STATIC_DRAW));
    GLCall(glVertexArrayElementBuffer(m_vaoId, m_iboId));

	// adding atomic buffer
	//GLCall(glGenBuffers(1, &m_atomicBufferId));
}

void Sequence::run()
{
    Timer timer;

    LOG(debug) << "Rendering...";

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
                    GLCall(glActiveTexture(GL_TEXTURE0 + textureIndex));
                    GLCall(glBindTexture(GL_TEXTURE_2D, textureId));

                    LOG(debug) << "Texture " << textureId << " binding is " << textureIndex;
                }
                else
                {
                    GLCall(glActiveTexture(GL_TEXTURE0 + textureIndex));
                    GLCall(glBindTexture(GL_TEXTURE_2D, textureId));
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, input.tex_min);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, input.tex_mag);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, input.tex_s);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, input.tex_t);

                if (input.tex_min != GL_LINEAR && input.tex_min != GL_NEAREST) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000); // Default LOD level is 1000
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0);
                    GLCall(glGenerateTextureMipmap(textureId)); // NOTE: OpenGL 4.5+
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
            case GL_FLOAT:
            case GL_FLOAT_VEC2:
            case GL_FLOAT_VEC3:
            case GL_FLOAT_VEC4:
                input.uniform->set(&input.floats[0]);
                break;
            case GL_DOUBLE:
            case GL_DOUBLE_VEC2:
            case GL_DOUBLE_VEC3:
            case GL_DOUBLE_VEC4:
                input.uniform->set(&input.doubles[0]);
                break;
            default:
                input.uniform->set(&input.floats[0]);
                break;
            }
        }
        //
        // Internal uniforms
        
        // Frame buffer size
        GLint uniform_loc = glGetUniformLocation(pass.program->getId(), "iFBsize");
        GLCall(glUniform2uiv(uniform_loc, 1, reinterpret_cast<unsigned int*>(pass.size)));
        // Frame buffer aspect ratio
        uniform_loc = glGetUniformLocation(pass.program->getId(), "iFBaspect");
        GLCall(glUniform1f(uniform_loc, pass.size[0] / (float)pass.size[1]));
        // Quad boolean
        uniform_loc = glGetUniformLocation(pass.program->getId(), "isQuad");
        GLCall(glUniform1i(uniform_loc, pass.meshes.begin()->second.mesh.isQuad));
        
//
// Atomic buffers
//

        // Setup atomic counters list

        auto& pass_acounters = pass.program->get_m_acounters();
        
        for (auto& inputCounterIt : pass.inputCounters)
        {
            // check if the counter counterName is used in the shader
            auto passCounter = pass_acounters.find(inputCounterIt.first);

            if (passCounter == pass_acounters.end())
			{
				LOG(warning) << "Atomic counter " << inputCounterIt.first << " is not used in the shader";
				exit(-1);
			}

            auto u_counterIt = pass.u_aCounters.insert({ passCounter->second->binding, passCounters()});
            u_counterIt->second.buffer = passCounter->second;

            u_counterIt->second.value.resize(passCounter->second->size);
            u_counterIt->second.result.resize(passCounter->second->size);

            auto p_countIt = p_aCounters.insert({ passCounter->second->binding, m_passCounters() });
           
            p_countIt->second.buffer = passCounter->second;
            
            p_countIt->second.value.resize(passCounter->second->size);
            p_countIt->second.result.resize(passCounter->second->size);
            
            if (inputCounterIt.second.value.size() > u_counterIt->second.value.size())
            {
				LOG(warning) << "Atomic counter " << inputCounterIt.first << " has more values than the shader";
				exit(-1);
			}
            
            std::memcpy(u_counterIt->second.value.data(), inputCounterIt.second.value.data(), passCounter->second->size * sizeof(GLuint));
            std::memcpy(p_countIt->second.value.data(), inputCounterIt.second.value.data(), passCounter->second->size * sizeof(GLuint));

            //u_counterIt->second.passIn = pass.fboId;
            u_counterIt->second.passIn = pass.fboId;
            p_countIt->second.passIn.insert({ pass.fboId, true });

            passCounter->second->userInput = true;
            
        }

        //for (auto& counterIt : pass_acounters)
        GLint bindingID = -1;
        bool cntrGroup = false;

        for ( std::pair<const std::string, std::shared_ptr<GLProgramBuffers>> counterIt : pass_acounters)
		{
            if (counterIt.second->userInput)
                continue;

            if (bindingID == counterIt.second->binding) {
                cntrGroup = true;
            } else {
                bindingID = counterIt.second->binding;
                cntrGroup = false;
			}

            std::multimap<GLint, passCounters>::iterator u_counterIt = pass.u_aCounters.insert({ counterIt.second->binding, passCounters() });
            
            // check if the binding already used
            //auto bindedCouters = p_aCounters.equal_range(counterIt.second->binding);
            //if (bindedCouters.first != bindedCouters.second) {
            int check = checkCounters( counterIt ); // 0 - not found, 1 - same binding, 2 - same binding and offset, 3 - identical
            
            // get shader counter binding offset and size and check it it overlap or identical

			std::map<GLint, m_passCounters>::iterator p_countIt;
            switch (check) { // no binding
            case 0:
            case 1:
                p_countIt = p_aCounters.insert({ counterIt.second->binding, m_passCounters() });
                p_countIt->second.buffer = counterIt.second;
                p_countIt->second.value.resize(counterIt.second->size);
                p_countIt->second.result.resize(counterIt.second->size);
                p_countIt->second.passIn.insert({ pass.fboId, false });
                cntrGroup = true;
                break;
            case 3: // binding, offset and size the same - skip
                break;
            case 2: // binding and offset the same but size different
                LOG(error) << "Atomic counter " << counterIt.first << " in pass " << pass.fboId << " have different size than in other passes";
                //exit(-1);
            }

            u_counterIt->second.buffer = counterIt.second;

            u_counterIt->second.value.resize(counterIt.second->size);
            u_counterIt->second.result.resize(counterIt.second->size);

            u_counterIt->second.passIn = pass.fboId;
            

            LOG(trace) << "Atomic counter " << counterIt.first << " binding is " << counterIt.second->binding << std::endl;
		}

        // Binding atomic counters
        std::cout << termcolor::bright_green;
        LOG(trace) << "Binding atomic counters" << std::endl;
        std::cout << termcolor::reset;

        auto it = pass.u_aCounters.begin();

        while (it != pass.u_aCounters.end()) {
            // Check if pass is not a first pass
            // Check if counter is not set
            // CHeck if counter do not have already used binding point
            
            //if (pass.fboId > 1) {
            //    break;
            //}

            GLuint buff_size = 0;
            //GLuint atomicCounterBufferID;

            auto range = pass.u_aCounters.equal_range(it->first);

            size_t range_size = std::distance(range.first, range.second);
            LOG(trace) << "Binding: " << it->first << " have " << range_size << " counter[s]." << std::endl;

            // Bind the buffer to the binding point
            
            GLCall(glGenBuffers(range_size, &it->second.bufferID));
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));
            
            for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
                // if new offset + counter size is bigger than buffer size than increase buffer size
                GLuint groupSize = groupIt->second.buffer->offset + groupIt->second.buffer->size * sizeof(GLuint);
                buff_size = std::max(buff_size, groupSize);
                LOG(trace) << groupIt->second.buffer->name << " buff_size: " << buff_size / sizeof(GLuint) << std::endl;
            }
            // Allocate the buffer with null data
            GLCall(glBufferData(GL_ATOMIC_COUNTER_BUFFER, buff_size, nullptr, GL_DYNAMIC_DRAW));

            // set the value of the counter / per counter
            for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
                auto buffer = groupIt->second.buffer;
                GLCall(glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, buffer->offset, sizeof(GLuint) * buffer->size, groupIt->second.value.data()));
                LOG(trace) << buffer->name << " offset: " << buffer->offset << " size: " << buffer->size << std::endl;

                groupIt->second.bufferID = it->second.bufferID;
                buffer->isSet = true;
            }

            GLCall(glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, it->first, it->second.bufferID, 0, buff_size));

            //auto pass_counter = p_acounters.insert({ it->first, {it->second.bufferID , it->second.buffer }});

            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

            it = range.second;
        }

        GLCall(glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT));

#if 0 //_DEBUG
        std::cout << termcolor::bright_yellow;
        LOG(trace) << "Check Binding atomic counters" << std::endl;
        std::cout << termcolor::reset;

        it = pass.u_aCounters.begin();

        while (it != pass.u_aCounters.end()) {
            std::vector<GLint> boundBuffer(it->second.size);
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));
            GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, it->second.offset, it->second.size * sizeof(GLuint), boundBuffer.data()));

            LOG(trace) << it->second.name << " offset: " << it->second.offset << " size: " << it->second.size;
            for (const auto& elem : boundBuffer) { std::cout << elem << ' '; }

            std::cout << std::endl;

            // Unbind the buffer after checking
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

            it++;
        } 
#endif
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

            std::vector<GLenum> buffers(8, GL_NONE); // 8 is the maximum number of color attachments

            for (auto& outputIt : pass.outputs)
            {
                auto& output = outputIt.second;
                buffers[output.output->location] = GL_COLOR_ATTACHMENT0 + output.output->location;
            }

//
// atomic buffers
//
            GLuint ssbo;
            GLuint bindingPoint = 3;
            const GLint bufSize = 3;
            // make GLuint array bufSize with 0
            GLint initValues[bufSize] = { 0,65535,0 };
            GLCall(glGenBuffers(1, &ssbo));
            GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo));
            GLCall(glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * bufSize, initValues, GL_DYNAMIC_DRAW));

            GLCall(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, ssbo));

            Pass::SSBObject ssbObject(GL_SHADER_STORAGE_BUFFER, ssbo, bindingPoint, bufSize, initValues);
            pass.glbObject.SSBO.insert({ "AtBuf",ssbObject });

            // 

            //
            GLuint depthBuffer;
            GLCall(glCreateRenderbuffers(1, &depthBuffer));
            GLCall(glNamedRenderbufferStorage(depthBuffer, GL_DEPTH_COMPONENT, pass.size[0], pass.size[1]));
            GLCall(glNamedFramebufferRenderbuffer(pass.fboId, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer));

            GLCall(glDrawBuffers((GLsizei)buffers.size(), &buffers[0]));

            GLCall(glViewport(0, 0, pass.size[0], pass.size[1]));

            GLCall(glClearColor(pass.clearColor[0], pass.clearColor[1], pass.clearColor[2], pass.clearColor[3]));

            // Enable depth Buffering and test
            GLCall(glEnable(GL_DEPTH_TEST));
            GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
            GLCall(glDepthFunc(GL_LEQUAL));


            // for this moment mesh only one for all passes
            Pass* defPass = &m_passes[0];
            MeshInput* crntMesh = &defPass->meshes.begin()->second;

            //auto& m_vaoId = crntMesh.VBO.vaoId;
            GLCall(glBindVertexArray(crntMesh->VBO.vaoId));

            GLCall(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
            GLCall(glDrawElements(crntMesh->mesh.render, crntMesh->mesh.numIndxs, GL_UNSIGNED_INT, 0));

            // get error code if any
            GLenum err = glGetError();
            if (err != GL_NO_ERROR)
            {
                LOG(error) << "OpenGL error: " << err;
                exit(1);
            }

            //  Atomic counters get values after draw
            //GLCall(glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT));
            //GLCall(glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT));

            std::cout << termcolor::bright_cyan;
            LOG(trace) << "Atomic counters results:" << std::endl;
            std::cout << termcolor::reset;

// here
            pass = m_passes[0];

            it = pass.u_aCounters.begin();

            while (it != pass.u_aCounters.end()) {
                auto buffer = it->second.buffer;
                std::vector<GLint> boundBuffer(buffer->size);
                GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));
                GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, buffer->offset, buffer->size * sizeof(GLuint), boundBuffer.data()));

                LOG(trace) << buffer->name << " offset: " << buffer->offset << " size: " << buffer->size;
                for (const auto& elem : boundBuffer) { std::cout << elem << ' '; }

                std::cout << std::endl;

                // Unbind the buffer after checking
                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

                it++;
            }
#if 0
            for (auto& u_counterIt : pass.u_aCounters ) {
				GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, u_counterIt.second.bufferID));
                u_counterIt.second.result.resize(u_counterIt.second.size);
                GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * u_counterIt.second.size, u_counterIt.second.result.data()));
                LOG(info) << termcolor::bright_yellow <<
                    "Atomic counter " << u_counterIt.first << " = " << u_counterIt.second.result[0] << termcolor::reset;
			}

                        //GLuint counterValue1, counterValue2;
            auto counterIt = pass.glbObject.BO.find("counter1");
            if (counterIt == pass.glbObject.BO.end()) {
                LOG(error) << "counter1 not found";
                exit(1);
            };
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterIt->second.id));
            GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 2 * sizeof(GLuint), counterIt->second.value));

            auto counterIt2 = pass.glbObject.BO.find("counter2");
            if (counterIt2 == pass.glbObject.BO.end()) {
                LOG(error) << "counter2 not found";
                exit(1);
            };
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterIt2->second.id));
            GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), counterIt2->second.value));
            std::cout << "Max britgtness: " << static_cast<float>(counterIt->second.value[0]) / 65535.0f << std::endl;
            std::cout << "Min britgtness: " << static_cast<float>(counterIt->second.value[1]) / 65535.0f << std::endl;
            std::cout << "Count         : " << static_cast<unsigned int>(*counterIt2->second.value) << std::endl;

#endif
            auto aBuffIt = pass.glbObject.SSBO.find("AtBuf");
            if (aBuffIt == pass.glbObject.SSBO.end()) {
                LOG(error) << "AtBuf not found";
                exit(1);
            };
            GLuint bufferId = aBuffIt->second.id;
            GLuint bufferSize = aBuffIt->second.size;
            GLint* bufferValue = aBuffIt->second.value;

            GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferId));
            GLCall(glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * bufferSize, bufferValue));
            std::cout << "Atomic buffer [0]: " << bufferValue[0] << std::endl;
            std::cout << "Atomic buffer [1]: " << bufferValue[1] << std::endl;
            std::cout << "Atomic buffer [2]: " << static_cast<float>( bufferValue[2] ) / 65535.0f << std::endl;

            GLCall(glDeleteBuffers(1, &ssbo));
            //GLCall(glDeleteBuffers(1, &counterObuff));
            //GLCall(glDeleteBuffers(1, &counterBuffer2));
 //
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

    // Destroying all passess atomicCounterBufferID
    for (auto& pass : m_passes) {
        auto counterIt = pass.u_aCounters.begin();
        while (counterIt != pass.u_aCounters.end()) {
            auto range = pass.u_aCounters.equal_range(counterIt->first);

            size_t range_size = std::distance(range.first, range.second);
            if (range_size > 1) {
                counterIt = range.second;
                if (counterIt != pass.u_aCounters.begin()) {
                    --counterIt;
                }
            }
            GLCall(glDeleteBuffers(1, &counterIt->second.bufferID));
            ++counterIt;
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    LOG(debug) << "Sequence completed in " << timer.nowText();
}

#define STRING_USED_DEFAULTS "(used default)"
#define STRING_CHANGED_TO_SUPPORTED "(changed to highest supported value for file format)"
