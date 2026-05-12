// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.


#include "program.h"
#include "log.h"
#include "gl_utils.h"

#include <sstream>
#include <fstream>

#include <termcolor/termcolor.hpp>

namespace {
[[noreturn]] void
throw_program_error(const std::string& message)
{
    throw std::runtime_error(message);
}

void
trim_glsl_array_suffix(char* name, GLsizei length)
{
    char* split = std::find(name, name + length, '[');
    if (split != name + length) {
        *split = '\0';
    }
}

GLsizei
uniform_component_count(const GLenum type)
{
    switch (type) {
    case GL_BOOL:
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
    case GL_DOUBLE: return 1;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2:
    case GL_UNSIGNED_INT_VEC2:
    case GL_FLOAT_VEC2:
    case GL_DOUBLE_VEC2: return 2;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3:
    case GL_UNSIGNED_INT_VEC3:
    case GL_FLOAT_VEC3:
    case GL_DOUBLE_VEC3: return 3;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4:
    case GL_UNSIGNED_INT_VEC4:
    case GL_FLOAT_VEC4:
    case GL_DOUBLE_VEC4:
    case GL_FLOAT_MAT2:
    case GL_DOUBLE_MAT2: return 4;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2:
    case GL_DOUBLE_MAT2x3:
    case GL_DOUBLE_MAT3x2: return 6;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2:
    case GL_DOUBLE_MAT2x4:
    case GL_DOUBLE_MAT4x2: return 8;
    case GL_FLOAT_MAT3:
    case GL_DOUBLE_MAT3: return 9;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3:
    case GL_DOUBLE_MAT3x4:
    case GL_DOUBLE_MAT4x3: return 12;
    case GL_FLOAT_MAT4:
    case GL_DOUBLE_MAT4: return 16;
    default: return 1;
    }
}
}  // namespace

//
// Uniform
//

// Int Uniforms
void
GLProgramUniform::set(GLint value)
{
    if (isSet) {
        if (ints[0] == value)
            return;
    } else
        isSet = true;

    ints[0] = value;

    switch (type) {
    case GL_SAMPLER_2D:
    case GL_IMAGE_2D:
    case GL_BOOL:
    case GL_INT:
        glUniform1i(location, value);
        break;
        //default:
        //    assert(0);
        //    break;
    }
}
void
GLProgramUniform::set(const GLint* values)
{
    int i;
    const GLsizei valueCount = uniform_component_count(type);

    if (isSet) {
        for (i = 0; i < valueCount; i++) {
            if (ints[i] != values[i])
                break;
        }

        if (i == valueCount)
            return;
    } else
        isSet = true;

    for (i = 0; i < valueCount; i++)
        ints[i] = values[i];

    switch (type) {
    case GL_BOOL:
    case GL_INT: glUniform1iv(location, 1, values); break;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2: glUniform2iv(location, 1, values); break;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3: glUniform3iv(location, 1, values); break;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4: glUniform4iv(location, 1, values); break;
    }
}
// Unsigned Int Uniforms
void
GLProgramUniform::set(GLuint value)
{
    if (isSet) {
        if (uints[0] == value)
            return;
    } else
        isSet = true;

    uints[0] = value;

    switch (type) {
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_IMAGE_2D:
        glUniform1i(location, static_cast<GLint>(value));
        break;
    case GL_UNSIGNED_INT:
        glUniform1ui(location, value);
        break;
        //default:
        //    assert(0);
        //    break;
    }
}
void
GLProgramUniform::set(const GLuint* values)
{
    int i;
    const GLsizei valueCount = uniform_component_count(type);

    if (isSet) {
        for (i = 0; i < valueCount; i++) {
            if (uints[i] != values[i])
                break;
        }

        if (i == valueCount)
            return;
    } else
        isSet = true;

    for (i = 0; i < valueCount; i++)
        uints[i] = values[i];

    switch (type) {
    case GL_UNSIGNED_INT: glUniform1uiv(location, 1, values); break;
    case GL_UNSIGNED_INT_VEC2: glUniform2uiv(location, 1, values); break;
    case GL_UNSIGNED_INT_VEC3: glUniform3uiv(location, 1, values); break;
    case GL_UNSIGNED_INT_VEC4: glUniform4uiv(location, 1, values); break;
    }
}
// Float Unifroms
void
GLProgramUniform::set(GLfloat value)
{
    if (isSet) {
        if (floats[0] == value)
            return;
    } else
        isSet = true;

    floats[0] = value;

    switch (type) {
    case GL_FLOAT: glUniform1f(location, value); break;
    }
}
void
GLProgramUniform::set(const GLfloat* values)
{
    int i;
    const GLsizei valueCount = uniform_component_count(type);

    if (isSet) {
        for (i = 0; i < valueCount; i++) {
            if (floats[i] != values[i])
                break;
        }

        if (i == valueCount)
            return;
    } else
        isSet = true;

    for (i = 0; i < valueCount; i++)
        floats[i] = values[i];

    // setting uniform variables in current program
    switch (type) {
    case GL_FLOAT: glUniform1fv(location, 1, values); break;
    case GL_FLOAT_VEC2: glUniform2fv(location, 1, values); break;
    case GL_FLOAT_VEC3: glUniform3fv(location, 1, values); break;
    case GL_FLOAT_VEC4: glUniform4fv(location, 1, values); break;
    case GL_FLOAT_MAT2: glUniformMatrix2fv(location, 1, false, values); break;
    case GL_FLOAT_MAT2x3: glUniformMatrix2x3fv(location, 1, false, values); break;
    case GL_FLOAT_MAT2x4: glUniformMatrix2x4fv(location, 1, false, values); break;
    case GL_FLOAT_MAT3: glUniformMatrix3fv(location, 1, false, values); break;
    case GL_FLOAT_MAT3x2: glUniformMatrix3x2fv(location, 1, false, values); break;
    case GL_FLOAT_MAT3x4: glUniformMatrix3x4fv(location, 1, false, values); break;
    case GL_FLOAT_MAT4: glUniformMatrix4fv(location, 1, false, values); break;
    case GL_FLOAT_MAT4x2: glUniformMatrix4x2fv(location, 1, false, values); break;
    case GL_FLOAT_MAT4x3: glUniformMatrix4x3fv(location, 1, false, values); break;
    }
}
// Double Unifroms
void
GLProgramUniform::set(GLdouble value)
{
    if (isSet) {
        if (doubles[0] == value)
            return;
    } else
        isSet = true;

    doubles[0] = value;

    switch (type) {
    case GL_DOUBLE: glUniform1d(location, value); break;
    }
};
void
GLProgramUniform::set(const GLdouble* values)
{
    int i;
    const GLsizei valueCount = uniform_component_count(type);

    if (isSet) {
        for (i = 0; i < valueCount; i++) {
            if (doubles[i] != values[i])
                break;
        }

        if (i == valueCount)
            return;
    } else
        isSet = true;

    for (i = 0; i < valueCount; i++)
        doubles[i] = values[i];

    // setting uniform variables in current program
    switch (type) {
    case GL_DOUBLE: glUniform1dv(location, 1, values); break;
    case GL_DOUBLE_VEC2: glUniform2dv(location, 1, values); break;
    case GL_DOUBLE_VEC3: glUniform3dv(location, 1, values); break;
    case GL_DOUBLE_VEC4: glUniform4dv(location, 1, values); break;
    case GL_DOUBLE_MAT2: glUniformMatrix2dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT2x3: glUniformMatrix2x3dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT2x4: glUniformMatrix2x4dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT3: glUniformMatrix3dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT3x2: glUniformMatrix3x2dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT3x4: glUniformMatrix3x4dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT4: glUniformMatrix4dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT4x2: glUniformMatrix4x2dv(location, 1, false, values); break;
    case GL_DOUBLE_MAT4x3: glUniformMatrix4x3dv(location, 1, false, values); break;
    }
}

//
// Shader
//

GLShader::GLShader(GLenum type, const std::string& data)
    : type(type)
    , isValid(false)
{
    id = glCreateShader(type);

    const char* source = data.c_str();
    const int length   = (int)data.length();

    GLCall(glShaderSource(id, 1, &source, &length));
    GLCall(glCompileShader(id));

    finalize();
}

GLShader::GLShader(GLenum type, const std::vector<char>& data)
    : type(type)
    , isValid(false)
{
    id = glCreateShader(type);

    GLCall(glShaderBinary(1, &id, GL_SHADER_BINARY_FORMAT_SPIR_V, data.data(), (int)data.size()));
    GLCall(glSpecializeShader(id, "main", 0, nullptr, nullptr));

    finalize();
}

GLShader::~GLShader()
{
    if (id) {
        //LOG(debug) << "~GLShader()";
        glDeleteShader(id);
    }
}

void
GLShader::finalize()
{
    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);

    if (!result) {
        GLsizei length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

        GLchar* infoLog = new GLchar[length];
        glGetShaderInfoLog(id, length, &length, infoLog);

        std::string typeName;

        switch (type) {
        case GL_VERTEX_SHADER: typeName = "Vertex"; break;
        case GL_FRAGMENT_SHADER: typeName = "Fragment"; break;
        case GL_COMPUTE_SHADER: typeName = "Compute"; break;
        default: assert(0); break;
        }

        LOG(error) << typeName << " shader has compilation error(s):" << std::endl << infoLog << std::endl;
        delete[] infoLog;

        return;
        //exit(1);
    }

    isValid = true;
}

//
// Program
//

GLProgram::GLProgram(const std::vector<std::shared_ptr<GLShader>>& shaders)
    : m_isValid(false)
{
    LOG(debug) << "Creating program from a shader set.";

    m_id = glCreateProgram();

    for (auto& s : shaders) {
        if (!s->isValid)
            return;

        GLCall(glAttachShader(m_id, s->id));
    }

    GLCall(glLinkProgram(m_id));

    // TODO: Support this.
    //GLCall(glValidateProgram(m_id));

    GLint status;
    glGetProgramiv(m_id, GL_LINK_STATUS, &status);

    if (!status) {
        GLsizei length;
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &length);

        GLchar* infoLog = new GLchar[length];
        glGetProgramInfoLog(m_id, length, &length, infoLog);

        LOG(error) << "Program has linking error(s):" << std::endl << infoLog << std::endl;
        delete[] infoLog;

        //exit(1);
        return;
    }

#if 0
	// TODO: It is mostly hints from implementation.
	GLCall(glValidateProgram(id));
	GLCall(glGetProgramiv(id, GL_VALIDATE_STATUS, &status);

	if (status != 0)
	{
		LOG(info) << "Shader has validation errors.");
	exit(1);
	}
#endif

    LOG(debug) << "Program has linked successfully.";

#if _DEBUG
    DebugShaderVarList();
#endif

    compileUniformList();
    compileBuffersList();
    compileOutputList();

    m_isValid = true;
}

GLProgram::~GLProgram()
{
    if (m_id)
        glDeleteProgram(m_id);
}

GLProgramUniform*
GLProgram::findUniform(const std::string& name)
{
    auto it = m_uniforms.find(name);

    if (it == m_uniforms.end())
        return nullptr;

    return &it->second;
}

//GLProgramAttributes* GLProgram::findAttributes(const std::string& name)
//{
//    auto it = m_attributes.find(name);
//
//    if (it == m_attributes.end())
//        return nullptr;
//
//    return &it->second;
//}

std::shared_ptr<GLProgramBuffers>
GLProgram::findCounter(std::string name)
{
    auto it = m_acounters.find(name);

    if (it == m_acounters.end())
        return nullptr;

    return it->second;
}

size_t
GLProgram::BuffersSize()
{
    size_t it = m_acounters.size();

    return it;
}

GLProgramOutput*
GLProgram::findOutput(const std::string& name)
{
    auto it = m_outputs.find(name);
    if (it == m_outputs.end())
        return nullptr;

    return &it->second;
}

// compile a list of user-defined uniforms
void
GLProgram::compileUniformList()
{
    int count;

    GLCall(glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count));

    if (!count) {
        m_uniforms.clear();
        return;
    }

    for (int i = 0; i < count; i++) {
        char name[256] = {};
        GLsizei length;
        const GLenum props[] = { GL_TYPE,
                                 GL_LOCATION,
                                 GL_ARRAY_SIZE,
                                 GL_BLOCK_INDEX,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER };
        GLint propData[7];

        GLCall(glGetProgramResourceName(m_id, GL_UNIFORM, i, sizeof(name), &length, name));
        GLCall(glGetProgramResourceiv(m_id, GL_UNIFORM, i, 7, props, 7, NULL, propData));

        if (length > sizeof(name)) {
            throw_program_error("program: uniform name length exceeds 256");
        }

        const GLenum type   = static_cast<GLenum>(propData[0]);
        const GLint location = propData[1];
        const GLint size     = propData[2];
        const GLint block    = propData[3];

        if (location == -1 || block != -1) {
            // internal uniforms
            // atomic counters
            continue;
        }

        std::string uniformName;
        if (length <= 0 || name[0] == '\0') {
            uniformName = "__uniform_" + std::to_string(location);
        } else {
            trim_glsl_array_suffix(name, length);
            uniformName = name;
        }

        const char* type_name = glsl_type_name(type);
        m_uniforms.insert({ uniformName, GLProgramUniform(type, type_name, location, size) });
        LOG(trace) << type_name << ": " << uniformName << " location = " << location;
    }
}

// compile a list of shader defined atomic buffers
void
GLProgram::compileBuffersList()
{
    int acount,  // acount - number of atomic counter buffers from glGetProgramiv(GL_ACTIVE_ATOMIC_COUNTER_BUFFERS)
        bcount,  // bcount - number of shader storage blocks from glGetProgramInterfaceiv(GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES)
        ncount,  // ncount - number of uniforms from glGetProgramInterfaceiv(GL_UNIFORM, GL_ACTIVE_RESOURCES)
        bncount;  // bncount - number of buffer variables from glGetProgramInterfaceiv(GL_BUFFER_VARIABLE, GL_ACTIVE_RESOURCES)

    m_acounters.clear();
    m_abuffers.clear();

    // returns the number of active attribute atomic counter buffers used by program.
    GLCall(glGetProgramiv(m_id, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &acount));
    GLCall(glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &bcount));
    GLCall(glGetProgramInterfaceiv(m_id, GL_BUFFER_VARIABLE, GL_ACTIVE_RESOURCES, &bncount));

    struct cntrNames {
        std::string name;
        GLint buffer_index;
        GLuint offset, size;
        cntrNames(std::string name, GLint buffer_index, GLuint offset, GLuint size)
            : name(name)
            , buffer_index(buffer_index)
            , offset(offset)
            , size(size)
        {
        }
    };
    std::vector<cntrNames> counters_list;

    struct cntrBinding {
        GLint binding = -1;
        GLsizei size  = 0;
    };
    std::vector<cntrBinding> counters_bindings;

    // buffers_map - multimap to store buffer info.
    struct bufNames  // struct to store buffer names and offset in buffers_map
    {
        std::string name;
        GLenum type;
        GLuint offset, size;
        bufNames(std::string name, GLenum type, GLuint offset, GLuint size)
            : name(name)
            , type(type)
            , offset(offset)
            , size(size)
        {
        }
    };
    std::multimap<int, bufNames> buffers_map;
    struct bufBinding  // struct to store buffer binding point
    {
        std::string name;
        GLuint binding, size;
        bufBinding(std::string name, GLuint binding, GLuint size)
            : name(name)
            , binding(binding)
            , size(size)
        {
        }
    };

    std::multimap<GLint, bufBinding> buffers_bindings;

    if (acount > 0) {
        GLCall(glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &ncount));
        counters_list.reserve(ncount);

        for (int i = 0; i < ncount; i++) {
            char name[256];
            GLsizei length;

            const GLenum props[] = { GL_TYPE,
                                     GL_LOCATION,
                                     GL_ARRAY_SIZE,
                                     GL_OFFSET,
                                     GL_ATOMIC_COUNTER_BUFFER_INDEX,
                                     GL_REFERENCED_BY_VERTEX_SHADER,
                                     GL_REFERENCED_BY_FRAGMENT_SHADER,
                                     GL_REFERENCED_BY_COMPUTE_SHADER };
            GLint propData[8];

            GLCall(glGetProgramResourceName(m_id, GL_UNIFORM, i, sizeof(name), &length, name));
            GLCall(glGetProgramResourceiv(m_id, GL_UNIFORM, i, 8, props, 8, NULL, propData));

            if (length > sizeof(name)) {
                throw_program_error("program: uniform name length exceeds 256");
            }

            if (propData[0] != GL_UNSIGNED_INT_ATOMIC_COUNTER) {
                continue;
            }

            trim_glsl_array_suffix(name, length);

            counters_list.push_back(cntrNames(name, propData[4], propData[3], propData[2]));
        }

        counters_bindings.resize(acount);

        for (GLint i = 0; i < acount; i++) {
            GLint binding, size;

            GLCall(glGetActiveAtomicCounterBufferiv(m_id, i, GL_ATOMIC_COUNTER_BUFFER_BINDING, &binding));
            GLCall(glGetActiveAtomicCounterBufferiv(m_id, i, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE, &size));

            counters_bindings[i].binding = binding;
            counters_bindings[i].size    = size;
        }

        for (const cntrNames& counter : counters_list) {
            if (counter.buffer_index < 0 || counter.buffer_index >= static_cast<GLint>(counters_bindings.size())) {
                throw_program_error("Atomic counter " + counter.name + " references invalid buffer index "
                                    + std::to_string(counter.buffer_index));
            }

            const cntrBinding& bindingInfo = counters_bindings[counter.buffer_index];
            const GLint counterBytes       = static_cast<GLint>(counter.size * sizeof(GLuint));

            if (bindingInfo.binding < 0 || bindingInfo.size < counter.offset + counterBytes) {
                throw_program_error("Atomic counter " + counter.name + " size does not fit reflected buffer size.");
            }

            LOG(trace) << "Counter: " << counter.name << " binding: " << bindingInfo.binding
                       << " offset: " << counter.offset << " size: " << counter.size << std::endl;

            m_acounters.insert({ counter.name,
                                 std::make_shared<GLProgramBuffers>(GLProgramBuffers::AtomicCounterBuffer(
                                     counter.name, counter.buffer_index, bindingInfo.binding, counter.offset,
                                     counter.size)) });
        }
    }

    if (!bcount || !bncount) {
        LOG(debug) << "Program: " << m_id << " Atomic buffers: " << m_abuffers.size() << std::endl;
        LOG(debug) << "Program: " << m_id << " Atomic counters: " << m_acounters.size() << std::endl;
        return;
    }

    // GL_SHADER_STORAGE_BUFFER - bindings scan pass

    for (int i = 0; i < bcount; i++) {
        char name[64];
        GLsizei length;

        const GLenum props[] = { GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES, GL_ACTIVE_VARIABLES };
        GLint propData[4];

        GLCall(glGetProgramResourceName(m_id, GL_SHADER_STORAGE_BLOCK, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            throw_program_error("program: buffer name length exceeds 64");
        }

        GLCall(glGetProgramResourceiv(m_id, GL_SHADER_STORAGE_BLOCK, i, 4, props, 4, nullptr, propData));
        // uniformInfo(std::string name, GLenum type, GLuint binding, GLuint offset, GLuint size)
        buffers_bindings.insert({ i, bufBinding(name, propData[0], propData[1]) });
        int count = propData[1] / sizeof(GLuint);
        LOG(trace) << "Buffer #" << i << ": " << name << ": binding = " << propData[0] << " size = " << count;
    }

    // GL_SHADER_STORAGE_BUFFER - Names scan pass
    int prv_index = -1;  // previous index
    int map_index = -1;  // map index
    int index     = -1;  // index in map

    for (int i = 0; i < bncount; i++) {
        char name[64];
        GLsizei length;
        const GLenum props[] = { GL_TYPE,
                                 GL_ARRAY_SIZE,
                                 GL_OFFSET,
                                 GL_BLOCK_INDEX,
                                 GL_ARRAY_STRIDE,
                                 GL_MATRIX_STRIDE,
                                 GL_IS_ROW_MAJOR,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER,
                                 GL_TOP_LEVEL_ARRAY_SIZE,
                                 GL_TOP_LEVEL_ARRAY_STRIDE };
        GLint propData[12];

        GLCall(glGetProgramResourceName(m_id, GL_BUFFER_VARIABLE, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            throw_program_error("program: uniform name length exceeds 64");
        }

        GLCall(glGetProgramResourceiv(m_id, GL_BUFFER_VARIABLE, i, 12, props, 12, nullptr, propData));
        //std::cout << "#" << i << " Name: " << name << std::endl
        //    << "Type: " << glsl_type_name(propData[0]) << std::endl   // GL_TYPE
        //    << "Size: " << propData[1] 					            // GL_ARRAY_SIZE
        //    << " Offset: " << propData[2] << std::endl; 				// GL_OFFSET

        map_index = propData[3];
        if (map_index != prv_index) {
            prv_index = map_index;
            index++;
        }
        trim_glsl_array_suffix(name, length);

        buffers_map.insert({
            index,                                                 // atomic counter buffer index
            bufNames(name, propData[0], propData[2], propData[1])  // name, type, offset, size
        });

        LOG(trace) << "#" << index << " (" << name << ") block index: " << map_index << " offset: " << propData[2]
                   << " size: " << propData[1] << " type: " << glsl_type_name(propData[0]) << std::endl;
    }

    index = 0;
    // GL_SHADER_STORAGE_BUFFER - matching bindings and names pass
    for (auto it : buffers_bindings) {
        //std::string name = it.second.name;
        //map_index = it.first;
        auto range = buffers_map.equal_range(it.first);
        for (auto jt = range.first; jt != range.second; ++jt) {
            //int size = jt->second.size;
            m_abuffers.insert({ it.second.name,
                                { jt->second.name, GLProgramBuffers(jt->second.name, jt->second.type, it.second.binding,
                                                                    jt->second.offset, jt->second.size) } });
            LOG(trace) << "#" << index << " " << it.second.name << " (" << jt->second.name
                       << ") binding: " << it.second.binding << " offset: " << jt->second.offset
                       << " size: " << jt->second.size << " type: " << glsl_type_name(jt->second.type) << std::endl;
            index++;
        }
    }
    LOG(debug) << "Program: " << m_id << " Atomic buffers: " << m_abuffers.size() << std::endl;
    LOG(debug) << "Program: " << m_id << " Atomic counters: " << m_acounters.size() << std::endl;
};

// compile a list of last program stage outputs
void
GLProgram::compileOutputList()
{
    GLint count;
    GLCall(glGetProgramInterfaceiv(m_id, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &count));

    if (!count) {
        m_outputs.clear();
        return;
    }

    for (int i = 0; i < count; i++) {
        char name[64] = {};
        GLsizei length;

        const GLenum props[] = { GL_TYPE,
                                 GL_LOCATION,
                                 GL_ARRAY_SIZE,
                                 GL_LOCATION_INDEX,
                                 GL_LOCATION_COMPONENT,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER };
        GLint propData[8];

        GLCall(glGetProgramResourceiv(m_id, GL_PROGRAM_OUTPUT, i, 8, props, 8, nullptr, propData));

        glGetProgramResourceName(m_id, GL_PROGRAM_OUTPUT, i, sizeof(name), &length, name);

        if (length > sizeof(name)) {
            throw_program_error("program: output name length exceeds 64");
        }

        std::string outputName;
        if (length <= 0 || name[0] == '\0') {
            if (propData[1] >= 0) {
                outputName = "__output_" + std::to_string(propData[1]);
            } else {
                outputName = "__output_index_" + std::to_string(i);
            }
        } else {
            trim_glsl_array_suffix(name, length);
            outputName = name;
        }

        m_outputs.insert({ outputName, GLProgramOutput(propData[0], propData[1], propData[2]) });
        LOG(trace) << "Output: " << outputName << " location = " << propData[1] << " type = " << glsl_type_name(propData[0])
                   << std::endl;
    }
}


void
GLProgram::DebugShaderVarList()
{
    std::cout << "Program ID: " << m_id << std::endl;

    // glGetProgramInterfaceiv
    GLint count;

    struct Resources {
        GLuint index;
        GLsizei propCount;
        const GLenum props[30] = {
            GL_NAME_LENGTH,                           // 0
            GL_TYPE,                                  // 1
            GL_ARRAY_SIZE,                            // 2
            GL_OFFSET,                                // 3
            GL_BLOCK_INDEX,                           // 4
            GL_ARRAY_STRIDE,                          // 5
            GL_MATRIX_STRIDE,                         // 6
            GL_IS_ROW_MAJOR,                          // 7
            GL_ATOMIC_COUNTER_BUFFER_INDEX,           // 8
            GL_TEXTURE_BUFFER,                        // 9
            GL_BUFFER_BINDING,                        // 10
            GL_BUFFER_DATA_SIZE,                      // 11
            GL_NUM_ACTIVE_VARIABLES,                  // 12
            GL_ACTIVE_VARIABLES,                      // 13
            GL_REFERENCED_BY_VERTEX_SHADER,           // 14
            GL_REFERENCED_BY_TESS_CONTROL_SHADER,     // 15
            GL_REFERENCED_BY_TESS_EVALUATION_SHADER,  // 16
            GL_REFERENCED_BY_GEOMETRY_SHADER,         // 17
            GL_REFERENCED_BY_FRAGMENT_SHADER,         // 18
            GL_REFERENCED_BY_COMPUTE_SHADER,          // 19
            GL_NUM_COMPATIBLE_SUBROUTINES,            // 20
            GL_COMPATIBLE_SUBROUTINES,                // 21
            GL_TOP_LEVEL_ARRAY_SIZE,                  // 22
            GL_TOP_LEVEL_ARRAY_STRIDE,                // 23
            GL_LOCATION,                              // 24
            GL_LOCATION_INDEX,                        // 25
            GL_IS_PER_PATCH,                          // 26
            GL_LOCATION_COMPONENT,                    // 27
            GL_TRANSFORM_FEEDBACK_BUFFER_INDEX,       // 28
            GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE,      // 29
        };
        GLsizei bufSize;
        GLsizei* length;
        GLint* params;
    } resources;

    GLint name_length;
    GLint max_num;
    std::cout << termcolor::bold << termcolor::bright_white;
    std::cout << "#==========================#" << std::endl;
    std::cout << "|        GL_UNIFORM        |" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_MAX_NAME_LENGTH, &name_length));
    std::cout << "| Max name lenght: " << name_length << std::endl;

    for (int i = 0; i < count; i++) {
        char name[64];
        GLsizei length;
        const GLenum props[] = { GL_TYPE,
                                 GL_LOCATION,
                                 GL_ARRAY_SIZE,
                                 GL_OFFSET,
                                 GL_ATOMIC_COUNTER_BUFFER_INDEX,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER };
        GLint propData[8];

        GLCall(glGetProgramResourceName(m_id, GL_UNIFORM, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_UNIFORM, i, 8, props, 8, nullptr, propData));

        std::cout << "#======== UNIFORM =========#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Name:     " << name << std::endl;
        std::cout << "| Type:     " << glsl_type_name(propData[0]) << std::endl;  // GL_TYPE
        std::cout << "| Location: " << propData[1] << std::endl;                  // GL_LOCATION
        std::cout << "| Size:     " << propData[2] << std::endl;                  // GL_ARRAY_SIZE
        std::cout << "| Offset:   " << propData[3] << std::endl;                  // GL_OFFSET
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Buffer index: " << propData[4] << std::endl;  // GL_ATOMIC_COUNTER_BUFFER_INDEX
        std::cout << "#==========================#" << std::endl;
        std::cout << "|        Referenced        |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|   vs   |   fs   |   cs   |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|    " << propData[5] << "   |    " << propData[6] << "   |    " << propData[7] << "   |"
                  << std::endl;
        std::cout << "#==========================#" << std::endl;
    };

    std::cout << termcolor::bright_yellow;
    std::cout << "#==========================#" << std::endl;
    std::cout << "|     GL_PROGRAM_INPUT     |" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_PROGRAM_INPUT, GL_MAX_NAME_LENGTH, &name_length));
    std::cout << "| Max name lenght: " << name_length << std::endl;

    for (int i = 0; i < count; i++) {
        char name[64];
        GLsizei length;
        const GLenum props[] = { GL_TYPE,
                                 GL_LOCATION,
                                 GL_ARRAY_SIZE,
                                 GL_LOCATION_COMPONENT,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER };
        GLint propData[7];

        GLCall(glGetProgramResourceName(m_id, GL_PROGRAM_INPUT, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_PROGRAM_INPUT, i, 7, props, 7, nullptr, propData));

        std::cout << "#========== INPUT =========#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Name:     " << name << std::endl;
        std::cout << "| Type:     " << glsl_type_name(propData[0]) << std::endl;
        std::cout << "| Location: " << propData[1] << std::endl;
        std::cout << "| Size:     " << propData[2] << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Location cmpnt: " << propData[3] << std::endl;
        std::cout << "#==========================#" << std::endl;
        std::cout << "|        Referenced        |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|   vs   |   fs   |   cs   |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|    " << propData[4] << "   |    " << propData[5] << "   |    " << propData[6] << "   |"
                  << std::endl;
        std::cout << "#==========================#" << std::endl;
    };

    std::cout << termcolor::bright_cyan;
    std::cout << "#==========================#" << std::endl;
    std::cout << "|    GL_PROGRAM_OUTPUT     |" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_PROGRAM_OUTPUT, GL_MAX_NAME_LENGTH, &name_length));
    std::cout << "| Max name lenght: " << name_length << std::endl;

    for (int i = 0; i < count; i++) {
        char name[64];
        GLsizei length;

        const GLenum props[] = { GL_TYPE,
                                 GL_LOCATION,
                                 GL_ARRAY_SIZE,
                                 GL_LOCATION_INDEX,
                                 GL_LOCATION_COMPONENT,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER };
        GLint propData[8];

        GLCall(glGetProgramResourceName(m_id, GL_PROGRAM_OUTPUT, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_PROGRAM_OUTPUT, i, 8, props, 8, nullptr, propData));

        std::cout << "#========= OUTPUT =========#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Name:     " << name << std::endl;
        std::cout << "| Type:     " << glsl_type_name(propData[0]) << std::endl;
        std::cout << "| Location: " << propData[1] << std::endl;
        std::cout << "| Size:     " << propData[2] << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Location indx: " << propData[3] << std::endl;
        std::cout << "| Location cmpnt: " << propData[4] << std::endl;
        std::cout << "#==========================#" << std::endl;
        std::cout << "|        Referenced        |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|   vs   |   fs   |   cs   |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|    " << propData[5] << "   |    " << propData[6] << "   |    " << propData[7] << "   |"
                  << std::endl;
        std::cout << "#==========================#" << std::endl;
    };

    std::cout << termcolor::bright_green;
    std::cout << "#==========================#" << std::endl;
    std::cout << "| GL_ATOMIC_COUNTER_BUFFER |" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_ATOMIC_COUNTER_BUFFER, GL_MAX_NUM_ACTIVE_VARIABLES, &max_num));
    std::cout << "| Max num active vars: " << max_num << std::endl;

    for (int i = 0; i < count; i++) {
        const GLenum props[] = { GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES, GL_ACTIVE_VARIABLES };
        GLint propData[4];

        GLCall(glGetProgramResourceiv(m_id, GL_ATOMIC_COUNTER_BUFFER, i, 4, props, 4, nullptr, propData));

        std::cout << "#=  ATOMIC_COUNTER_BUFFER =#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Binding:     " << propData[0] << std::endl;
        std::cout << "| Data size:   " << propData[1] << std::endl;
        std::cout << "| Num active vars: " << propData[2] << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Active vars: " << propData[3] << std::endl;
        std::cout << "#--------------------------#" << std::endl;
    }
    GLCall(glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count));

    for (int i = 0; i < count; i++) {
        char name[64];
        GLsizei length;
        const GLenum props[] = { GL_TYPE,
                                 GL_LOCATION,
                                 GL_ARRAY_SIZE,
                                 GL_OFFSET,
                                 GL_ATOMIC_COUNTER_BUFFER_INDEX,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER };
        GLint propData[8];

        GLCall(glGetProgramResourceName(m_id, GL_UNIFORM, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_UNIFORM, i, 8, props, 8, nullptr, propData));

        if (propData[0] != GL_UNSIGNED_INT_ATOMIC_COUNTER) {
            continue;
        }

        std::cout << "#= Atomic counter uniform =#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Name:     " << name << std::endl;
        std::cout << "| Type:     " << glsl_type_name(propData[0]) << std::endl;  // GL_TYPE
        std::cout << "| Location: " << propData[1] << std::endl;                  // GL_LOCATION
        std::cout << "| Size:     " << propData[2] << std::endl;                  // GL_ARRAY_SIZE
        std::cout << "| Offset:   " << propData[3] << std::endl;                  // GL_OFFSET
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Buffer index: " << propData[4] << std::endl;  // GL_ATOMIC_COUNTER_BUFFER_INDEX
        std::cout << "#==========================#" << std::endl;
        std::cout << "|        Referenced        |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|   vs   |   fs   |   cs   |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|    " << propData[5] << "   |    " << propData[6] << "   |    " << propData[7] << "   |"
                  << std::endl;
        std::cout << "#==========================#" << std::endl;
    };

    // GL_TRANSFORM_FEEDBACK_VARYING
    std::cout << termcolor::bright_blue;
    std::cout << "#==========================#" << std::endl;
    std::cout << "|GL_TRANSFORM_FEEDBACK_VAR |" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_TRANSFORM_FEEDBACK_VARYING, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_TRANSFORM_FEEDBACK_VARYING, GL_MAX_NAME_LENGTH, &max_num));
    std::cout << "| Max name length: " << max_num << std::endl;


    // GL_SHADER_STORAGE_BLOCK
    std::cout << termcolor::bright_cyan;
    std::cout << "#==========================#" << std::endl;
    std::cout << "| GL_SHADER_STORAGE_BLOCK  |" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &max_num));
    std::cout << "| Max name length: " << max_num << std::endl;

    for (int i = 0; i < count; i++) {
        char name[64];
        GLsizei length;
        const GLenum props[] = { GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES, GL_ACTIVE_VARIABLES };
        GLint propData[4];

        GLCall(glGetProgramResourceName(m_id, GL_SHADER_STORAGE_BLOCK, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_SHADER_STORAGE_BLOCK, i, 4, props, 4, nullptr, propData));
        std::cout << "#== SHADER_STORAGE_BLOCK ==#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Name:     " << name << std::endl;
        std::cout << "| Binding:     " << propData[0] << std::endl;
        std::cout << "| Data size:   " << propData[1] << std::endl;
        std::cout << "| Active vars: " << propData[2] << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Active vars: " << propData[3] << std::endl;
        std::cout << "#--------------------------#" << std::endl;
    }

    // GL_BUFFER_VARIABLE
    std::cout << termcolor::bright_white;
    std::cout << "#==========================#" << std::endl;
    std::cout << "#=== GL_BUFFER_VARIABLE ===#" << std::endl;
    std::cout << "#==========================#" << std::endl;

    GLCall(glGetProgramInterfaceiv(m_id, GL_BUFFER_VARIABLE, GL_ACTIVE_RESOURCES, &count));
    std::cout << "| Active: " << count << std::endl;
    GLCall(glGetProgramInterfaceiv(m_id, GL_BUFFER_VARIABLE, GL_MAX_NAME_LENGTH, &max_num));
    std::cout << "| Max name length: " << max_num << std::endl;

    for (int i = 0; i < count; i++) {
        char name[64];
        GLsizei length;
        const GLenum props[] = { GL_TYPE,
                                 GL_ARRAY_SIZE,
                                 GL_OFFSET,
                                 GL_BLOCK_INDEX,
                                 GL_ARRAY_STRIDE,
                                 GL_MATRIX_STRIDE,
                                 GL_IS_ROW_MAJOR,
                                 GL_REFERENCED_BY_VERTEX_SHADER,
                                 GL_REFERENCED_BY_FRAGMENT_SHADER,
                                 GL_REFERENCED_BY_COMPUTE_SHADER,
                                 GL_TOP_LEVEL_ARRAY_SIZE,
                                 GL_TOP_LEVEL_ARRAY_STRIDE };
        GLint propData[12];

        GLCall(glGetProgramResourceName(m_id, GL_BUFFER_VARIABLE, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_BUFFER_VARIABLE, i, 12, props, 12, nullptr, propData));
        std::cout << "#=== GL_BUFFER_VARIABLE ===#" << std::endl;
        std::cout << "| #" << i << std::endl << "| Name:     " << name << std::endl;
        std::cout << "| Type:     " << glsl_type_name(propData[0]) << std::endl;  // GL_TYPE
        std::cout << "| Size:     " << propData[1] << std::endl;                  // GL_ARRAY_SIZE
        std::cout << "| Offset:   " << propData[2] << std::endl;                  // GL_OFFSET
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Block index: " << propData[3] << std::endl;    // GL_BLOCK_INDEX
        std::cout << "| Array stride: " << propData[4] << std::endl;   // GL_ARRAY_STRIDE
        std::cout << "| Matrix stride: " << propData[5] << std::endl;  // GL_MATRIX_STRIDE
        std::cout << "| Row Major: " << propData[6] << std::endl;      // GL_IS_ROW_MAJOR
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "| Top level size: " << propData[10] << std::endl;    // GL_TOP_LEVEL_ARRAY_SIZE
        std::cout << "| Top level stride: " << propData[11] << std::endl;  // GL_TOP_LEVEL_ARRAY_STRIDE
        std::cout << "#==========================#" << std::endl;
        std::cout << "|        Referenced        |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|   vs   |   fs   |   cs   |" << std::endl;
        std::cout << "#--------------------------#" << std::endl;
        std::cout << "|    " << propData[7] << "   |    " << propData[8] << "   |    " << propData[9] << "   |"
                  << std::endl;
        std::cout << "#==========================#" << std::endl;
    }

    std::cout << termcolor::reset << std::endl;


    ////
    std::cout << "== GL_UNIFORM_BLOCK ==" << std::endl;
    glGetProgramInterfaceiv(m_id, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &count);
    std::cout << "GL_ACTIVE_RESOURCES: " << count << std::endl;
    glGetProgramInterfaceiv(m_id, GL_UNIFORM_BLOCK, GL_MAX_NAME_LENGTH, &count);
    std::cout << "GL_MAX_NAME_LENGTH: " << count << std::endl;
    glGetProgramInterfaceiv(m_id, GL_UNIFORM_BLOCK, GL_MAX_NUM_ACTIVE_VARIABLES, &count);
    std::cout << "GL_MAX_NUM_ACTIVE_VARIABLES: " << count << std::endl << std::endl;

    std::cout << "== GL_ATOMIC_COUNTER_BUFFER ==" << std::endl;
    glGetProgramInterfaceiv(m_id, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &count);
    std::cout << "GL_ACTIVE_RESOURCES: " << count << std::endl;

    glGetProgramInterfaceiv(m_id, GL_ATOMIC_COUNTER_BUFFER, GL_MAX_NUM_ACTIVE_VARIABLES, &count);
    std::cout << "GL_MAX_NUM_ACTIVE_VARIABLES: " << count << std::endl << std::endl;

    std::cout << "== GL_BUFFER_VARIABLE ==" << std::endl;
    glGetProgramInterfaceiv(m_id, GL_BUFFER_VARIABLE, GL_ACTIVE_RESOURCES, &count);
    std::cout << "GL_ACTIVE_RESOURCES: " << count << std::endl;
    glGetProgramInterfaceiv(m_id, GL_BUFFER_VARIABLE, GL_MAX_NAME_LENGTH, &count);
    std::cout << "GL_MAX_NAME_LENGTH: " << count << std::endl << std::endl;

    std::cout << "== GL_SHADER_STORAGE_BLOCK ==" << std::endl;
    glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count);
    std::cout << "GL_ACTIVE_RESOURCES: " << count << std::endl;
    glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &count);
    std::cout << "GL_MAX_NAME_LENGTH: " << count << std::endl;
    glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_MAX_NUM_ACTIVE_VARIABLES, &count);
    std::cout << "GL_MAX_NUM_ACTIVE_VARIABLES: " << count << std::endl << std::endl;

    std::cout << termcolor::reset;
}
