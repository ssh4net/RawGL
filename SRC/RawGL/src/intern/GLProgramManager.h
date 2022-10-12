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
