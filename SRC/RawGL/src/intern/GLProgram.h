#pragma once

#include "Common.h"
#include "OpenGLUtils.h"

struct GLProgramUniform
{
    GLenum type;
    GLint location;
    GLsizei size;

    // avoid redundant GL calls if current & provided values are the same
    GLint ints[4];
    GLfloat floats[16];

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
	GLenum type;
	bool isValid;

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
