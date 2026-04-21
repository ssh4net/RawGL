// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022 Erium Vladlen.

#pragma once

#include "common.h"
#include "program.h"
#include "asset_manager.h"
#include "rawgl/rawgl_core.h"

class GLProgramManager : public AssetManager<GLProgram> {
public:
    GLProgramManager() {}

    // From a single text file with stage-guarded source sections
    // Text-only because SPIR-V shaders are always in separate files
    std::shared_ptr<GLProgram> loadVertFrag(const std::string& path);

    // From two separate text/binary files
    std::shared_ptr<GLProgram> loadVertFrag(const std::string paths[]);

    // From source strings
    std::shared_ptr<GLProgram> loadVertFragStrings(const std::string& name, const std::string sources[]);

    // From structured module definitions
    std::shared_ptr<GLProgram> loadVertFragModules(const std::string& name,
                                                   const std::vector<rawgl::ShaderModuleDefinition>& modules);

    // From a single text/binary file
    std::shared_ptr<GLProgram> loadComp(const std::string& path);

    // From a source string
    std::shared_ptr<GLProgram> loadCompString(const std::string& name, const std::string& source);

    // From a structured module definition
    std::shared_ptr<GLProgram> loadCompModule(const std::string& name, const rawgl::ShaderModuleDefinition& module);

private:
    //std::unique_ptr<GLShader> loadShader(const std::string& path, const std::string& macros = "");
    bool loadTextFile(const std::string& path, std::string& out);
    bool loadBinaryFile(const std::string& path, std::vector<char>& out);
};
