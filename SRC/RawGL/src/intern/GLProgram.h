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
#include "OpenGLUtils.h"

struct GLProgramUniform
{
    GLenum type;
	std::string type_name;
    GLint location;
    GLsizei size;

    // avoid redundant GL calls if current & provided values are the same
	GLint ints[4] = { 0, 0, 0, 0 };
	GLuint uints[4] = { 0, 0, 0, 0 };
    GLfloat floats[16] = {
        0.0f, 0.0f, 0.0f, 0.0f, 
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };
	GLdouble doubles[16] = {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	};

    bool isSet;

    GLProgramUniform(GLenum type, std::string type_name, GLint location, GLsizei size) : type(type), type_name(type_name), location(location), size(size), isSet(false) {}

    void set(GLint value);
    void set(const GLint *values);
	void set(GLuint value);
	void set(const GLuint* values);
    void set(GLfloat value);
    void set(const GLfloat* values);
	void set(GLdouble value);
	void set(const GLdouble* values);
};

struct GLProgramAttributes
{
	GLenum type;
	std::string type_name;
	GLint location;
	GLsizei size = 0;

	// avoid redundant GL calls if current & provided values are the same
	GLuint value = 0;

	bool isSet;
	
	GLProgramAttributes(GLenum type, std::string type_name, GLint location, GLsizei size) : type(type), type_name(type_name), location(location), size(size), isSet(false) {}
	
	void set(GLuint value);
};

struct GLProgramBuffers
{
	std::string name = "";
	GLenum type;
	std::string type_name;
	GLint index, binding, offset, location;
    GLsizei size = 0;

	bool isSet;
	bool userInput;

	GLProgramBuffers() :
		name(""),
		type(0), type_name(""),
		index(0),binding(0),offset(0),location(0),size(0),
		isSet(false), userInput(false)
	{};

	GLProgramBuffers(std::string name, GLenum type, GLint binding, GLint size) : 
		name(name), type(type), binding(binding), size(size), isSet(false), userInput(false),
		index(0), offset(0), location(0)
	{
		type_name = static_cast<std::string>(glsl_type_name(type));
	}

	GLProgramBuffers(std::string name, GLenum type, GLint binding, GLint offset, GLint size) :
		name(name), type(type), binding(binding), offset(offset), size(size), isSet(false), userInput(false),
		index(0), location(0)
	{
		type_name = static_cast<std::string>(glsl_type_name(type));
	}

	static GLProgramBuffers AtomicCounterBuffer(std::string name, GLint index, GLint binding, GLint offset, GLsizei size) {
		GLProgramBuffers buffer;
		buffer.name = name;
		buffer.type = GL_UNSIGNED_INT_ATOMIC_COUNTER;
		buffer.type_name = static_cast<std::string>(glsl_type_name(buffer.type));
		buffer.index = index;
		buffer.binding = binding;
		buffer.offset = offset;
		buffer.size = size;
		buffer.isSet = false;
		buffer.userInput = false;
		return buffer;
	}

	~GLProgramBuffers() = default;
};

struct GLProgramOutput
{
    GLenum type;
	std::string type_name;
	GLuint location;

    GLProgramOutput(GLuint location) : type(NULL), type_name(""), location(location) {}
	GLProgramOutput(GLenum type, GLuint location) 
		: type(type), location(location)
	{
		type_name = static_cast<std::string>(glsl_type_name(type));
	}
};

struct GLShader
{
	GLuint id;
	GLenum type = 0;
	bool isValid = true;

	GLShader() : id(0) {}

	// From source text
	GLShader(GLenum type, const std::string& data);

	// From SPIR-V binary code
	GLShader(GLenum type, const std::vector<char>& data);
	~GLShader();

private:
	void finalize();
};

class GLProgram
{
public:
	GLProgram(const std::vector<std::shared_ptr<GLShader>> &shaders);
	~GLProgram();

    GLProgramUniform* findUniform(const std::string& name);
	GLProgramAttributes* findAttributes(const std::string& name);
	std::shared_ptr<GLProgramBuffers> findCounter(std::string name);
	std::vector<GLProgramBuffers*> findBuffers(std::string name);
    GLProgramOutput* findOutput(const std::string& name);

	size_t BuffersSize();

	// get compiled shader atomic counters
	std::map<std::string, std::shared_ptr<GLProgramBuffers>>& get_m_acounters()
	{
		return m_acounters;
	}

    GLuint getId() const { return m_id; }
	bool isValid() const { return m_isValid; }

private:
	std::map<std::string, GLProgramUniform> m_uniforms;
    std::map<std::string, std::shared_ptr<GLProgramBuffers>> m_acounters;
	std::multimap<std::string, std::pair<std::string, GLProgramBuffers>> m_abuffers; // < buffer_name , < buffer_var_name, buffer_parms > >
	std::map<std::string, GLProgramOutput> m_outputs;

	GLuint m_id;
	bool m_isValid;

    // Compile a list of user-defined uniforms
    void compileUniformList();

    // Compile a list of last program stage outputs
    void compileOutputList();

    // Compile a list of last program stage outputs
    void compileBuffersList();

	// Debug a list of shader variables
	void DebugShaderVarList();
};
