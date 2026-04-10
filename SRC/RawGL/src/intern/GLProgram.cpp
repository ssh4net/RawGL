/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
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


#include "GLProgram.h"
#include "Log.h"
#include "OpenGLUtils.h"

#include <sstream>
#include <fstream>

#include <termcolor/termcolor.hpp>

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

    if (isSet) {
        for (i = 0; i < size; i++) {
            if (ints[i] != values[i])
                break;
        }

        if (i == size)
            return;
    } else
        isSet = true;

    for (i = 0; i < size; i++)
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
    case GL_UNSIGNED_INT:
        glUniform1i(location, value);
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

    if (isSet) {
        for (i = 0; i < size; i++) {
            if (uints[i] != values[i])
                break;
        }

        if (i == size)
            return;
    } else
        isSet = true;

    for (i = 0; i < size; i++)
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

    if (isSet) {
        for (i = 0; i < size; i++) {
            if (floats[i] != values[i])
                break;
        }

        if (i == size)
            return;
    } else
        isSet = true;

    for (i = 0; i < size; i++)
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

    if (isSet) {
        for (i = 0; i < size; i++) {
            if (doubles[i] != values[i])
                break;
        }

        if (i == size)
            return;
    } else
        isSet = true;

    for (i = 0; i < size; i++)
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

    // returns the number of active attribute atomic counter buffers used by program.
    GLCall(glGetProgramiv(m_id, GL_ACTIVE_UNIFORMS, &count));
    // returns the number of active attribute atomic counter buffers used by program.
    //GlCall(glGetProgramiv(m_id, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &count));

    if (!count) {
        m_uniforms.clear();
        return;
    }

    for (int i = 0; i < count; i++) {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;

        GLCall(glGetActiveUniform(m_id, i, sizeof(name), &length, &size, &type, name));

        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLint location = glGetUniformLocation(m_id, name);

        const char* type_name = nullptr;

        //for (int j = 0; j < type_set_size; j++) {
        //    if (type_set[j].type != type)
        //        continue;
        //    type_name = type_set[j].name;
        //    break;
        //}

        type_name = glsl_type_name(type);

        if (location == -1) {
            // internal uniforms
            // atomic counters
            continue;
        }
        m_uniforms.insert({ name, GLProgramUniform(type, type_name, location, size) });
        LOG(trace) << type_name << ": " << name << " location = " << location;
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

    // returns the number of active attribute atomic counter buffers used by program.
    GLCall(glGetProgramiv(m_id, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &acount));
    GLCall(glGetProgramInterfaceiv(m_id, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &bcount));
    GLCall(glGetProgramInterfaceiv(m_id, GL_BUFFER_VARIABLE, GL_ACTIVE_RESOURCES, &bncount));

    if (!acount || !bcount) {
        m_acounters.clear();
        return;
    }

    // counters_map - multimap to store atomic counter info.
    // Index is atomic counter binding point.
    struct cntrNames  // struct to store atomic counter names and offset in counters_map
    {
        std::string name;
        GLuint offset, size;
        cntrNames(std::string name, GLuint offset, GLuint size)
            : name(name)
            , offset(offset)
            , size(size)
        {
        }
    };
    std::multimap<int, cntrNames> counters_map;

    // counters_bindings - map to store atomic counter binding point.
    // buffer_binding - map to store buffer binding point.
    std::multimap<GLint, GLsizei> counters_bindings;

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

    // atomic counters names scan pass
    GLCall(glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &ncount));
    for (int i = 0; i < ncount; i++) {
        char name[256];
        GLsizei length;
        GLsizei size;
        GLenum type;

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

        GLCall(glGetActiveUniform(m_id, i, sizeof(name), &length, &size, &type, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }
        if (type == GL_UNSIGNED_INT_ATOMIC_COUNTER) {
            counters_map.insert({
                propData[4],                               // atomic counter buffer index
                cntrNames(name, propData[3], propData[2])  // name, offset, size
            });
        }
    }
    int counter_idx    = 0;
    size_t uniform_cnt = counters_map.size();

    for (GLint i = 0; i < acount; i++) {
        GLint binding, size;

        GLCall(glGetActiveAtomicCounterBufferiv(m_id, i, GL_ATOMIC_COUNTER_BUFFER_BINDING, &binding));
        GLCall(glGetActiveAtomicCounterBufferiv(m_id, i, GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE, &size));

        counters_bindings.insert({ binding, size });
    }

    int map_index = 0;
    int index     = 0;

    // atomic counters bindings scan pass
    for (const auto& it : counters_bindings) {
        GLuint size = it.second / sizeof(GLuint);
        GLuint buffer_size;
        if (size > 1) {
            for (int i = 0; i < size; i++) {
                auto range = counters_map.equal_range(map_index);
                for (auto jt = range.first; jt != range.second; ++jt) {
                    buffer_size = size;
                    LOG(trace) << "Counter: " << jt->second.name << " binding: " << it.first
                               << " offset: " << jt->second.offset << " size: " << jt->second.size << std::endl;

                    m_acounters.insert(
                        { jt->second.name,
                          std::make_shared<GLProgramBuffers>(GLProgramBuffers::AtomicCounterBuffer(
                              jt->second.name, map_index, it.first, jt->second.offset, jt->second.size)) });
                    index++;
                    buffer_size -= jt->second.size + jt->second.offset / sizeof(GLuint);
                    if (buffer_size == 0) {
                        break;
                    }
                }
                if (buffer_size == 0) {
                    break;
                }
                LOG(error) << "Atomic counter and uniform size mizmatch.";
                exit(1);
            }
        } else {
            auto range = counters_map.equal_range(map_index);
            for (auto jt = range.first; jt != range.second; ++jt) {
                buffer_size = size;
                LOG(trace) << "Counter: " << jt->second.name << " binding: " << it.first
                           << " offset: " << jt->second.offset << " size: " << jt->second.size << std::endl;

                m_acounters.insert({ jt->second.name,
                                     std::make_shared<GLProgramBuffers>(GLProgramBuffers::AtomicCounterBuffer(
                                         jt->second.name, map_index, it.first, jt->second.offset, jt->second.size)) });
                index++;
                buffer_size -= jt->second.size + jt->second.offset / sizeof(GLuint);
            }
            if (buffer_size != 0) {
                LOG(error) << "Atomic counter and uniform size mizmatch.";
                exit(1);
            }
        }
        map_index++;
    }

    // GL_SHADER_STORAGE_BUFFER - bindings scan pass

    for (int i = 0; i < bcount; i++) {
        char name[64];
        GLsizei length;

        const GLenum props[] = { GL_BUFFER_BINDING, GL_BUFFER_DATA_SIZE, GL_NUM_ACTIVE_VARIABLES, GL_ACTIVE_VARIABLES };
        GLint propData[4];

        GLCall(glGetProgramResourceName(m_id, GL_SHADER_STORAGE_BLOCK, i, sizeof(name), &length, name));
        if (length > sizeof(name)) {
            LOG(error) << "program: buffer name length exceeds " << sizeof(name);
            exit(1);
        }

        GLCall(glGetProgramResourceiv(m_id, GL_SHADER_STORAGE_BLOCK, i, 4, props, 4, nullptr, propData));
        // uniformInfo(std::string name, GLenum type, GLuint binding, GLuint offset, GLuint size)
        buffers_bindings.insert({ i, bufBinding(name, propData[0], propData[1]) });
        int count = propData[1] / sizeof(GLuint);
        LOG(trace) << "Buffer #" << i << ": " << name << ": binding = " << propData[0] << " size = " << count;
    }

    // GL_SHADER_STORAGE_BUFFER - Names scan pass
    int prv_index = -1;  // previous index
    map_index     = -1;  // map index
    index         = -1;  // index in map

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
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
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
        char* split = std::find(name, name + length, '[');
        if (split != name + length) {
            *split = '\0';
        }

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

        GLCall(glGetProgramResourceiv(m_id, GL_PROGRAM_OUTPUT, i, 8, props, 8, nullptr, propData));

        glGetProgramResourceName(m_id, GL_PROGRAM_OUTPUT, i, sizeof(name), &length, name);

        if (length > sizeof(name)) {
            LOG(error) << "program: output name length exceeds " << sizeof(name);
            exit(1);
        }

        m_outputs.insert({ name, GLProgramOutput(propData[0], propData[1]) });
        LOG(trace) << "Output: " << name << " location = " << propData[1] << " type = " << glsl_type_name(propData[0])
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