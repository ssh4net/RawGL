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

#pragma once

#include "Common.h"
#include "Texture.h"
#include "ImageUtils.h"
#include "GLProgram.h"

#include <functional>

typedef enum class hres {
    OK = 0,
    ERR = 1,
} hres;

template <typename T>
T str_to_numeric(hres& hr, const std::string& str_val);

template<> int32_t str_to_numeric(hres& hr, const std::string& str_val);
template<> float_t str_to_numeric(hres& hr, const std::string& str_val);

template <typename T>
T str_to_numeric(const std::string& str_val);

template<> int32_t str_to_numeric(const std::string& str_val);
template<> float_t str_to_numeric(const std::string& str_val);

// 
// Uniform input (numeric or texture data)
// 
struct PassInput
{
    PassInput();

    //  Maximal number of integer values
    // (`GL_BOOL_VEC4` / `GL_INT_VEC4` at the moment)
    static constexpr uint8_t NUM_INTS = 4;

    //  Maximal number of float values
    // (`GL_FLOAT_MAT4` at the moment)
    static constexpr uint8_t NUM_FLOATS = 16;

    // Forward declaration of texture attribute structures
    struct TexAttrValue;
    struct TexAttr;

    // Texture attribute
    struct TexAttr
    {
        // Attribute name(min, mag, s, t, ...)
        std::string name;
        // Setter function (see `_pass_input_set_tex_min` as an example)
        std::function<void(PassInput&, const GLint&)> func;
        // Array of possible attribute values
        std::vector<TexAttrValue> possible_values;
        // String description (for documentation and help messages purposes)
        std::string desc;
    };

    //  See:
    // `TexAttr->possible_values`
    struct TexAttrValue
    {
        // String value key
        std::string key;
        // GLSL integer flag value it holds
        GLint gl_value;
        // String description (for documentation and help messages purposes)
        std::string desc;
    };

    friend const void _pass_input_set_tex_min(PassInput& pi, const GLint& val);
    friend const void _pass_input_set_tex_mag(PassInput& pi, const GLint& val);
    friend const void _pass_input_set_tex_s(PassInput& pi, const GLint& val);
    friend const void _pass_input_set_tex_t(PassInput& pi, const GLint& val);

    /// @brief Evaluates texture attribute from string attribute name and key value
    /// @param hr - Result handle
    /// @param name - Attribute name
    /// @param attr_val_name - Attribute value key name
    const void eval_tex_attr(hres& hr, const std::string& name, const std::string& attr_val_name);

    static std::string get_possible_tex_attr_fmt();

    static const std::vector<TexAttr> TEX_ATTR_ARR;

    /* Parse numeric values from array of strings.
    */

    /* TODO: Maybe make it private? */
    std::map<std::string, std::string> attributes;
    GLProgramUniform* uniform;
    std::shared_ptr<Texture> texture;
    GLint tex_min;
    GLint tex_mag;
    GLint tex_s;
    GLint tex_t;
    GLint ints[NUM_INTS];
    GLfloat floats[NUM_FLOATS];

    std::string path;
};

const void _pass_input_set_tex_min(PassInput& pi, const GLint& val);
const void _pass_input_set_tex_mag(PassInput& pi, const GLint& val);
const void _pass_input_set_tex_s(PassInput& pi, const GLint& val);
const void _pass_input_set_tex_t(PassInput& pi, const GLint& val);

struct PassInputCounters
{
	PassInputCounters();
	
    struct CounterParm;
    struct CounterParmValue;
	
    // Counters parameters
    struct CounterParm
    {
        // Attribute name(min, mag, s, t, ...)
        std::string name;
        // Setter function (see `_pass_input_set_tex_min` as an example)
        // std::function<void(PassInputCounters&, const GLint&)> func;
        // String description (for documentation and help messages purposes)
        std::string desc;
    };

    const void eval_counter_parm(hres& hr, const std::string& parm_name);
	
    static const std::vector<CounterParm> COUNTER_PARM_ARR;
	
    GLint binding;
	GLint offset;
	GLint size;
	GLint value;
    std::string path;
};

struct PassOutputCounters
{
    PassOutputCounters();

    std::string key;
    GLint ref_pass;
    GLint binding;
    GLint offset;
    GLint size;
    GLint value;
};

//
// File output
//
struct PassOutput
{
    //
    // Data gathered from output source (CLI or API calls)
    //

    // Framebuffer output texture format description
    std::string internalFormatText;

    // Path to output file
    std::string path;


    // User-provided OIIO & plugin attributes
    std::map<std::string, std::string> attributes;

    // Number of channels
    int channels;

    // Index of alpha channel
    int alphaChannel;

    // Bits per channel
    int bits;

    //std::pair<std::string, int> compression;

    //
    // Post-parse data
    //

    // Pointer to an output in pass' program object (fragment shader)
    GLProgramOutput* output;

    // Pointer to a uniform in pass' program object (compute shader)
    GLProgramUniform* uniform;

	// Pointer to a atomic counter in pass' program object (compute shader)
    GLProgramAtomicBuffers* atomicCounter;
	
    // Texture attached to this output channel in the FBO
    std::shared_ptr<Texture> texture;

    // OIIO stuff
    OIIO::TypeDesc format;
    bool formatDefaulted;

    PassOutput();

    void saveTexture();
};

struct Pass
{
    //
    // Data gathered from source (CLI or API calls)
    //

    std::shared_ptr<GLProgram> program;

    // Compute shaders
    bool isCompute;

    // One for each user-specified data entry
    std::map<std::string, PassInput> inputs;

	// One for each user-specified atomic counter
	std::map<std::string, PassInputCounters> atomicCounters;

    // One for each last stage shader output (i.e. fragment)
    std::map<std::string, PassOutput> outputs;

    // Size description
    std::string sizeText[2];

    // Threads description
    std::string workGroupSizeText[2];

    //
    // Post-parse data
    //

    // Output viewport size
	int size[2] = { 512, 512 };

    // Number of threads
	int workGroupSize[2] = { 16, 16 };

    GLuint fboId;

    Pass(const std::shared_ptr<GLProgram>& p, bool isCompute) :
        program(p),
        isCompute(isCompute),
        // TODO: Here because defaulted options only work with po::variables_map
        sizeText{ "512", "512" },
        workGroupSizeText{ "16", "16" },
        fboId(0)
    {}
};

class Sequence
{
public:
    // initialize from command line
    Sequence(int argc, const char* argv[]);
    ~Sequence();

    void run();

    // log the parsed chain to the console
    void dump();

private:
    Sequence() {}

    // all resources used in the sequence
    std::map<std::string, std::shared_ptr<Texture>> m_textures;
    //std::map<std::string, std::shared_ptr<GLSLProgram>> m_shaders;
    
	// ? maybe not needed ?
    std::map < std::string, std::shared_ptr<GLProgramAtomicBuffers>> m_counters;
    std::vector<Pass> m_passes;

    GLuint m_vaoId, m_vboId, m_tboId, m_cboId, m_iboId;

    void initCommon();
};
