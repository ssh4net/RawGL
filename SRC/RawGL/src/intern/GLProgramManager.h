#pragma once

#include "Common.h"
#include "GLProgram.h"
#include "AssetManager.h"

class GLProgramManager :
	public AssetManager<GLProgram>
{
public:
	GLProgramManager() {}

	// From a single text file (shaders separated with macros)
	// Text-only because SPIR-V shaders are always in separate files
	std::shared_ptr<GLProgram> loadVertFrag(const std::string& path);

	// From two separate text/binary files
	std::shared_ptr<GLProgram> loadVertFrag(const std::string paths[]);

	// From source strings
	std::shared_ptr<GLProgram> loadVertFragStrings(const std::string& name, const std::string sources[]);

	// From a single text/binary file
	std::shared_ptr<GLProgram> loadComp(const std::string& path);

	// From a source string
	std::shared_ptr<GLProgram> loadCompString(const std::string& name, const std::string& source);

private:
	//std::unique_ptr<GLShader> loadShader(const std::string& path, const std::string& macros = "");
	bool loadTextFile(const std::string& path, std::string& out);
	bool loadBinaryFile(const std::string& path, std::vector<char>& out);
};

extern GLProgramManager g_glslProgramManager;
