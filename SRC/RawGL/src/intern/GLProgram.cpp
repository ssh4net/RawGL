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

#include "GLProgram.h"
#include "Log.h"

#include <sstream>
#include <fstream>

//
// Uniform
//

void GLProgramUniform::set(GLint value)
{
    if (isSet)
    {
        if (ints[0] == value)
            return;
    }
    else
        isSet = true;

    ints[0] = value;

    switch (type)
    {
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

void GLProgramUniform::set(const GLint *values)
{
    int i;

    if (isSet)
    {
        for (i = 0; i < size; i++)
        {
            if (ints[i] != values[i])
                break;
        }

        if (i == size)
            return;
    }
    else
        isSet = true;

    for (i = 0; i < size; i++)
        ints[i] = values[i];

    switch (type)
    {
    case GL_BOOL:
    case GL_INT:
        glUniform1iv(location, 1, values);
        break;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2:
        glUniform2iv(location, 1, values);
        break;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3:
        glUniform3iv(location, 1, values);
        break;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4:
        glUniform4iv(location, 1, values);
        break;
    }
}

void GLProgramUniform::set(GLfloat value)
{
    if (isSet)
    {
        if (floats[0] == value)
            return;
    }
    else
        isSet = true;
            
    floats[0] = value;

    switch (type)
    {
    case GL_FLOAT:
        glUniform1f(location, value);
        break;
    }
}
void GLProgramUniform::set(const GLfloat* values)
{
    int i;

    if (isSet)
    {
        for (i = 0; i < size; i++)
        {
            if (floats[i] != values[i])
                break;
        }

        if (i == size)
            return;
    }
    else
        isSet = true;

    for (i = 0; i < size; i++)
        floats[i] = values[i];

    switch (type)
    {
    case GL_FLOAT:
        glUniform1fv(location, 1, values);
        break;
    case GL_FLOAT_VEC2:
        glUniform2fv(location, 1, values);
        break;
    case GL_FLOAT_VEC3:
        glUniform3fv(location, 1, values);
        break;
    case GL_FLOAT_VEC4:
        glUniform4fv(location, 1, values);
        break;
    case GL_FLOAT_MAT2:
        glUniformMatrix2fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT2x3:
        glUniformMatrix2x3fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT2x4:
        glUniformMatrix2x4fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT3:
        glUniformMatrix3fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT3x2:
        glUniformMatrix3x2fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT3x4:
        glUniformMatrix3x4fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT4:
        glUniformMatrix4fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT4x2:
        glUniformMatrix4x2fv(location, 1, false, values);
        break;
    case GL_FLOAT_MAT4x3:
        glUniformMatrix4x3fv(location, 1, false, values);
        break;
    }
}

//
// Shader
//

GLShader::GLShader(GLenum type, const std::string& data) :
	type(type),
	isValid(false)
{
	id = glCreateShader(type);

	const char* source = data.c_str();
	const int length = (int)data.length();
	
	GLCall(glShaderSource(id, 1, &source, &length));
	GLCall(glCompileShader(id));

	finalize();
}

GLShader::GLShader(GLenum type, const std::vector<char>& data) :
	type(type),
	isValid(false)
{
	id = glCreateShader(type);

	GLCall(glShaderBinary(1, &id, GL_SHADER_BINARY_FORMAT_SPIR_V, data.data(), (int)data.size()));
	GLCall(glSpecializeShader(id, "main", 0, nullptr, nullptr));

	finalize();
}

GLShader::~GLShader()
{
	if (id)
	{
		//LOG(debug) << "~GLShader()";
		glDeleteShader(id);
	}
}

void GLShader::finalize()
{
	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);

	if (!result)
	{
		GLsizei length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

		GLchar* infoLog = new GLchar[length];
		glGetShaderInfoLog(id, length, &length, infoLog);

		std::string typeName;

		switch (type)
		{
		case GL_VERTEX_SHADER:
			typeName = "Vertex";
			break;
		case GL_FRAGMENT_SHADER:
			typeName = "Fragment";
			break;
		case GL_COMPUTE_SHADER:
			typeName = "Compute";
			break;
		default:
			assert(0);
			break;
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

GLProgram::GLProgram(const std::vector<std::shared_ptr<GLShader>>& shaders) :
	m_isValid(false)
{
	LOG(debug) << "Creating program from a shader set.";

	m_id = glCreateProgram();
	
	for (auto& s : shaders)
	{
		if (!s->isValid)
			return;

		GLCall(glAttachShader(m_id, s->id));
	}

	GLCall(glLinkProgram(m_id));
	
	// TODO: Support this.
	//GLCall(glValidateProgram(m_id));

	GLint status;
	glGetProgramiv(m_id, GL_LINK_STATUS, &status);

	if (!status)
	{
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

	compileUniformList();
	compileOutputList();

	m_isValid = true;
}

GLProgram::~GLProgram()
{
    if (m_id)
        glDeleteProgram(m_id);
}

GLProgramUniform* GLProgram::findUniform(const std::string& name)
{
    auto it = m_uniforms.find(name);

	if (it == m_uniforms.end())
        return nullptr;

    return &it->second;
}

GLProgramOutput* GLProgram::findOutput(const std::string& name)
{
    auto it = m_outputs.find(name);
    if (it == m_outputs.end())
        return nullptr;

    return &it->second;
}

// compile a list of user-defined uniforms
void GLProgram::compileUniformList()
{
    int count;
        
    // returns the number of active attribute atomic counter buffers used by program.
    GLCall(glGetProgramiv(m_id, GL_ACTIVE_UNIFORMS, &count));
    // returns the number of active attribute atomic counter buffers used by program.
    //GlCall(glGetProgramiv(m_id, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &count));

    if (!count)
    {
        m_uniforms.clear();
        return;
    }

    for (int i = 0; i < count; i++)
    {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;

        GLCall(glGetActiveUniform(m_id, i, sizeof(name), &length, &size, &type, name));

        if (length > sizeof(name))
        {
            LOG(error) << "program: uniform name length exceeds " << sizeof(name);
            exit(1);
        }

        GLint location = glGetUniformLocation(m_id, name);

        if (location == -1)
        {
            // internal uniform
            continue;
        }

        m_uniforms.insert({ name, GLProgramUniform(type, location, size) });
        LOG(trace) << "Uniform: " << name << " location = " << location;
    }
}

// compile a list of last program stage outputs
void GLProgram::compileOutputList()
{
    GLint count;
    GLCall(glGetProgramInterfaceiv(m_id, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &count));

    if (!count)
    {
        m_outputs.clear();
        return;
    }

    for (int i = 0; i < count; i++)
    {
        char name[64];
        GLsizei length;

        glGetProgramResourceName(m_id, GL_PROGRAM_OUTPUT, i, sizeof(name), &length, name);

        if (length > sizeof(name)) {
            LOG(error) << "program: output name length exceeds " << sizeof(name);
            exit(1);
        }

        //LOG(info) << "OUTPUT INDEX: %i %s\n\n", glGetProgramResourceIndex(m_id, GL_PROGRAM_OUTPUT, name), &name[0]);
        //m_outputs.insert({ name, ShaderOutput(glGetProgramResourceIndex(m_id, GL_PROGRAM_OUTPUT, name)) });
        //LOG(info) << "OUTPUT LOCATION: %i %s\n\n", glGetProgramResourceLocation(m_id, GL_PROGRAM_OUTPUT, name), &name[0]);
        m_outputs.insert({ name, GLProgramOutput(glGetProgramResourceLocation(m_id, GL_PROGRAM_OUTPUT, name)) });
        LOG(trace) << "Output: " << name << " location = " << glGetProgramResourceLocation(m_id, GL_PROGRAM_OUTPUT, name);
    }
}
