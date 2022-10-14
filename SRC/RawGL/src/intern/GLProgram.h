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
    GLint location;
    GLsizei size;

    // avoid redundant GL calls if current & provided values are the same
	GLint ints[4] = { 0, 0, 0, 0 };
    GLfloat floats[16] = {
        0.0, 0.0, 0.0, 0.0, 
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };

    bool isSet;

    GLProgramUniform(GLenum type, GLint location, GLsizei size) : type(type), location(location), size(size), isSet(false) {}

    void set(GLint value);
    void set(const GLint *values);
    void set(GLfloat value);
    void set(const GLfloat* values);
};

struct GLProgramOutput
{
    GLuint location;

    GLProgramOutput(GLuint location) : location(location) {}
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
    GLProgramOutput* findOutput(const std::string& name);

    GLuint getId() const { return m_id; }
	bool isValid() const { return m_isValid; }

private:
	std::map<std::string, GLProgramUniform> m_uniforms;
	std::map<std::string, GLProgramOutput> m_outputs;

	GLuint m_id;
	bool m_isValid;

    // Compile a list of user-defined uniforms
    void compileUniformList();

    // Compile a list of last program stage outputs
    void compileOutputList();
};
