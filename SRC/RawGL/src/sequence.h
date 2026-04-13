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

#include "common.h"
#include "texture.h"
#include "image_utils.h"
#include "gl_program.h"

#include <functional>

enum class hres {
    OK  = 0,
    ERR = 1,
};

template<typename T>
T
str_to_numeric(hres& hr, const std::string& str_val);

template<>
int32_t
str_to_numeric(hres& hr, const std::string& str_val);
template<>
uint32_t
str_to_numeric(hres& hr, const std::string& str_val);
template<>
float_t
str_to_numeric(hres& hr, const std::string& str_val);
template<>
double_t
str_to_numeric(hres& hr, const std::string& str_val);

template<typename T>
T
str_to_numeric(const std::string& str_val);

template<>
int32_t
str_to_numeric(const std::string& str_val);
template<>
uint32_t
str_to_numeric(const std::string& str_val);
template<>
float_t
str_to_numeric(const std::string& str_val);
template<>
double_t
str_to_numeric(const std::string& str_val);


struct Pass;

struct PassInput {
    PassInput();

    static constexpr uint8_t NUM_INTS    = 4;
    static constexpr uint8_t NUM_FLOATS  = 16;
    static constexpr uint8_t NUM_DOUBLES = 16;

    struct TexAttrValue;
    struct TexAttr;

    struct TexAttr {
        std::string name;
        std::function<void(PassInput&, const GLint&)> func;
        std::vector<TexAttrValue> possible_values;
        std::string desc;
    };

    struct TexAttrValue {
        std::string key;
        GLint gl_value;
        std::string desc;
    };

    friend const void _pass_input_set_tex_min(PassInput& pi, const GLint& val);
    friend const void _pass_input_set_tex_mag(PassInput& pi, const GLint& val);
    friend const void _pass_input_set_tex_s(PassInput& pi, const GLint& val);
    friend const void _pass_input_set_tex_t(PassInput& pi, const GLint& val);

    const void eval_tex_attr(hres& hr, const std::string& name, const std::string& attr_val_name);

    static std::string get_possible_tex_attr_fmt();

    static const std::vector<TexAttr> TEX_ATTR_ARR;

    std::map<std::string, std::string> attributes;
    GLProgramUniform* uniform;
    std::shared_ptr<Texture> texture;
    bool runtimeTextureBindingRequired = false;
    GLint tex_min;
    GLint tex_mag;
    GLint tex_s;
    GLint tex_t;
    GLint ints[NUM_INTS];
    GLuint uints[NUM_INTS];
    GLfloat floats[NUM_FLOATS];
    GLdouble doubles[NUM_DOUBLES];

    std::string path;
};

const void
_pass_input_set_tex_min(PassInput& pi, const GLint& val);
const void
_pass_input_set_tex_mag(PassInput& pi, const GLint& val);
const void
_pass_input_set_tex_s(PassInput& pi, const GLint& val);
const void
_pass_input_set_tex_t(PassInput& pi, const GLint& val);

struct MeshInput {
    struct Mesh;
    struct VertexBuffer;

    struct Mesh {
        bool isQuad;
        bool Triangles;
        GLuint render;
        std::string FileName;

        float* pVerts         = nullptr;
        float* pTexts         = nullptr;
        float* pNorms         = nullptr;
        unsigned char* pColrs = nullptr;
        uint32_t* pIndxs      = nullptr;

        GLsizei vrtSize, texSize, nrmSize, clrSize, idxSize, numIndxs;
    };
    Mesh mesh;

    struct VertexBuffer {
        GLuint vaoId = 0;
        GLuint vboId = 0;
        GLuint tboId = 0;
        GLuint nboId = 0;
        GLuint cboId = 0;
        GLuint iboId = 0;
    };
    VertexBuffer VBO;
    std::shared_ptr<struct SequenceSharedGpuMesh> sharedGpuMesh;

    struct MeshParmValue;

    struct MeshParm {
        std::string name;
        std::function<void(MeshInput&, const GLuint&)> func;
        std::vector<MeshParmValue> possible_values;
        std::string desc;
    };

    struct MeshParmValue {
        std::string key;
        GLuint gl_value;
        std::string desc;
    };

    friend const void _pass_input_set_triangles(MeshInput& pi, const GLuint& val);
    friend const void _pass_input_set_render(MeshInput& pi, const GLuint& val);

    const void eval_mesh_parm(hres& hr, const std::string& name, const std::string& attr_val_name);

    static std::string get_possible_mesh_parm_fmt();

    static const std::vector<MeshParm> MESH_PARM_ARR;

    MeshInput()
        : mesh { true, true, GL_TRIANGLES, "", nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0 }
        , VBO()
    {
    }
};

const void
_pass_input_set_triangles(MeshInput& pi, const GLuint& val);
const void
_pass_input_set_render(MeshInput& pi, const GLuint& val);

struct PassInputCounters {
    PassInputCounters();

    std::string name;
    GLuint bufferID;
    GLint binding;
    GLint offset;
    GLint size;
    std::vector<GLuint> value;
    std::vector<GLuint> result;

    int passIn;
};

struct passCounters {
    GLuint bufferID;

    std::vector<GLuint> value;
    std::vector<GLuint> result;

    std::shared_ptr<GLProgramBuffers> buffer;

    int passIn;

    passCounters()
        : bufferID(0)
        , value(0)
        , result(0)
        , buffer(nullptr)
        , passIn(-1)
    {
    }
};

struct PassOutput {
    std::string internalFormatText;
    std::string path;
    std::map<std::string, std::string> attributes;
    int channels;
    int alphaChannel;
    int bits;

    GLProgramOutput* output;
    GLProgramUniform* uniform;
    std::shared_ptr<Texture> texture;

    OIIO::TypeDesc format;
    bool formatDefaulted;

    PassOutput();

    void saveTexture();
};

struct Pass {
    template<typename T> struct GenericObject {
        std::string typetext = "";
        GLenum type;
        GLuint id, binding;
        const GLint size;
        T* value;

        GenericObject(GLenum type, GLuint id, GLuint binding, const GLint size, T* src_value = nullptr)
            : type(type)
            , id(id)
            , binding(binding)
            , size(size)
            , value(src_value)
        {
            typetext = "";
            value    = new T[size];
            if (src_value) {
                std::memcpy(value, src_value, size * sizeof(T));
            } else {
                std::memset(value, 0, size * sizeof(T));
            }
        }

        static T* create_default_values(size_t size)
        {
            T* default_values = new T[size];
            std::memset(default_values, 0, size * sizeof(T));
            return default_values;
        }

        ~GenericObject() {}
    };

    std::shared_ptr<GLProgram> program;

    struct FBOobject;
    struct VAObject;

    using BObject   = GenericObject<GLuint>;
    using SSBObject = GenericObject<GLint>;

    struct GLBO {
        std::vector<FBOobject> FBO;
        std::vector<VAObject> VBO;
        std::map<std::string, BObject> BO;
        std::map<std::string, SSBObject> SSBO;
    };
    GLBO glbObject;

    struct FBOobject {
        GLuint id;
        std::map<std::string, GLuint> object;
    };

    struct VAObject {
        GLuint id;
        std::map<std::string, GLuint> object;
    };

    bool isCompute;

    std::map<std::string, PassInput> inputs;

    struct inputCounter {
        GLuint size;
        std::vector<GLuint> value;

        inputCounter()
        {
            size  = 1;
            value = { 0 };
        }
    };

    std::map<std::string, inputCounter> inputCounters;
    std::map<std::string, std::vector<GLuint>> capturedAtomicCounterValues;
    std::multimap<GLint, passCounters> u_aCounters;
    std::map<std::string, PassInputCounters> u_aBuffers;
    std::map<std::string, PassOutput> outputs;
    std::map<std::string, MeshInput> meshes;

    struct CullMode {
        GLint windOrder     = GL_CW;
        GLint cullFace      = GL_BACK;
        bool cullFaceEnable = true;
    } cullMode;

    struct CullModeVal {
        std::string key;
        GLint gl_value;
        std::string desc;
    };

    struct CullModeAttr {
        std::string name;
        std::function<void(CullMode&, const GLuint&)> func;
        std::vector<CullModeVal> possible_values;
        std::string desc;
    };

    std::string sizeText[2];
    std::string workGroupSizeText[2];
    float clearColor[4];

    int size[2]          = { 512, 512 };
    int workGroupSize[2] = { 16, 16 };

    GLuint fboId;

    friend const void _pass_input_set_cull_face(CullMode& mm, const GLuint& val);
    friend const void _pass_input_set_wind_order(CullMode& mm, const GLuint& val);
    friend const void _pass_input_set_cull_enable(CullMode& mm, const GLuint& val);

    static std::string get_possible_culling_fmt();
    static const std::vector<Pass::CullModeAttr> CULL_PARM_ARR;

    const void eval_cull_parm(hres& hr, const std::string& name, const std::string& attr_val_name);

    Pass(const std::shared_ptr<GLProgram>& p, bool isCompute)
        : program(p)
        , isCompute(isCompute)
        , glbObject()
        , sizeText { "512", "512" }
        , workGroupSizeText { "16", "16" }
        , fboId(0)
        , clearColor { 0.0f, 0.0f, 0.0f, 0.0f }
        , cullMode { GL_CW, GL_BACK, true }
    {
    }
};

const void
_pass_input_set_cull_face(Pass::CullMode& mm, const GLuint& val);
const void
_pass_input_set_wind_order(Pass::CullMode& mm, const GLuint& val);
const void
_pass_input_set_cull_enable(Pass::CullMode& mm, const GLuint& val);

bool
Sequence_HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode);

struct SequenceParsedOption {
    std::string string_key;
    std::vector<std::string> value;
};

struct SequenceParsedArguments {
    bool showHelp    = false;
    bool showVersion = false;
    int verbosity    = 3;
    std::vector<SequenceParsedOption> options;
};

SequenceParsedArguments
Sequence_ParseArguments(int argc, const char* argv[]);

struct SequencePassConfig {
    std::shared_ptr<GLProgram> program;
    bool isCompute = false;
    std::map<std::string, PassInput> inputs;
    std::map<std::string, Pass::inputCounter> inputCounters;
    std::map<std::string, PassOutput> outputs;
    std::map<std::string, MeshInput> meshes;
    Pass::CullMode cullMode { GL_CW, GL_BACK, true };
    std::string sizeText[2] { "512", "512" };
    std::string workGroupSizeText[2] { "16", "16" };
    float clearColor[4] { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct SequenceBuildConfig {
    int verbosity = 3;
    std::vector<SequencePassConfig> passes;
};

struct SequenceRuntimePassConfig {
    std::shared_ptr<GLProgram> program;
    bool isCompute = false;
    std::map<std::string, PassInput> inputs;
    std::map<std::string, Pass::inputCounter> inputCounters;
    std::map<std::string, PassOutput> outputs;
    std::map<std::string, MeshInput> meshes;
    Pass::CullMode cullMode { GL_CW, GL_BACK, true };
    int size[2] { 512, 512 };
    int workGroupSize[2] { 16, 16 };
    float clearColor[4] { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct SequenceSharedMeshData {
    GLsizei vrtSize = 0;
    GLsizei texSize = 0;
    GLsizei nrmSize = 0;
    GLsizei clrSize = 0;
    GLsizei idxSize = 0;
    GLsizei numIndxs = 0;
    std::vector<float> verts;
    std::vector<float> texcoords;
    std::vector<float> normals;
    std::vector<unsigned char> colors;
    std::vector<uint32_t> indices;
};

struct SequenceSharedGpuMesh {
    MeshInput::VertexBuffer vertexBuffer;

    SequenceSharedGpuMesh() = default;
    ~SequenceSharedGpuMesh();

    SequenceSharedGpuMesh(const SequenceSharedGpuMesh&) = delete;
    SequenceSharedGpuMesh& operator=(const SequenceSharedGpuMesh&) = delete;
};

struct SequenceRuntimeConfig {
    int verbosity = 3;
    std::vector<SequenceRuntimePassConfig> passes;
    std::map<std::string, std::shared_ptr<Texture>> sharedTextures;
    std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> sharedMeshes;
    std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> sharedGpuMeshes;
};

struct SequenceSystemUniformState {
    double timeSeconds      = 0.0;
    double deltaTimeSeconds = 0.0;
    int frameNumber         = 0;
    int passIndex           = -1;
};

enum class SequenceExecutionInputOverrideKind {
    intValues,
    uintValues,
    floatValues,
    doubleValues,
    texture,
};

struct SequenceExecutionInputOverride {
    size_t passIndex = 0;
    std::string inputName;
    SequenceExecutionInputOverrideKind kind = SequenceExecutionInputOverrideKind::intValues;
    std::vector<int32_t> intValues;
    std::vector<uint32_t> uintValues;
    std::vector<float> floatValues;
    std::vector<double> doubleValues;
    std::shared_ptr<Texture> texture;
};

class Sequence {
public:
    Sequence(int argc, const char* argv[]);
    explicit Sequence(const SequenceParsedArguments& parsedArguments);
    explicit Sequence(const SequenceBuildConfig& buildConfig);
    explicit Sequence(const SequenceRuntimeConfig& runtimeConfig);
    ~Sequence();

    void run();
    void run(const SequenceSystemUniformState& systemUniforms);
    void run(const SequenceSystemUniformState& systemUniforms,
             const std::vector<SequenceExecutionInputOverride>& inputOverrides);
    std::shared_ptr<Texture> getPassOutputTexture(size_t passIndex, const std::string& outputName) const;
    std::vector<GLuint> getPassAtomicCounterValues(size_t passIndex, const std::string& counterName) const;
    void setPassAtomicCounterValues(size_t passIndex, const std::string& counterName, const std::vector<GLuint>& values);
    void releaseRunOutputTextures();

private:
    Sequence() {}

    struct ParseState {
        SequencePassConfig* currentPass = nullptr;
        PassInput* currentInput     = nullptr;
        PassOutput* currentOutput   = nullptr;
        MeshInput* currentMeshInput = nullptr;
        size_t currentPassN         = 0;

        std::string previousInternalFormatText = "rgb32f";
        int previousChannels                   = 3;
        int previousAlphaChannel               = -1;
        int previousBits                       = 16;
    };

    struct PlannedInputBinding {
        const std::string* name = nullptr;
        PassInput* input        = nullptr;
    };

    struct PlannedOutputBinding {
        PassOutput* output = nullptr;
    };

    struct PassExecutionPlan {
        Pass* pass                      = nullptr;
        const MeshInput* primaryMesh    = nullptr;
        int passIndex                   = -1;
        std::vector<PlannedInputBinding> inputs;
        std::vector<PlannedOutputBinding> outputs;
    };

    std::map<std::string, std::shared_ptr<Texture>> m_textures;
    std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> m_sharedMeshes;
    std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> m_sharedGpuMeshes;

    std::vector<SequencePassConfig> m_passConfigs;
    std::vector<Pass> m_passes;
    std::vector<PassExecutionPlan> m_executionPlan;

    void processParsedOption(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void processPassDeclaration(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void processPassProperty(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void processInputOption(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void processAtomicOption(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void processInputAttributeOption(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void processOutputOption(const std::string& key, const std::vector<std::string>& value, ParseState& state);
    void buildPassesFromConfig();
    void buildPassesFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig);
    void preloadInputTextures();
    void ensurePassOutputTextures(Pass& pass, int passIndex);
    void refreshPassTextureInputs(Pass& pass);
    void initializePass(Pass& pass, int passIndex);
    void validatePassSetup() const;
    void buildExecutionPlan();
    int bindPassInputs(const PassExecutionPlan& plan,
                       const std::vector<SequenceExecutionInputOverride>& inputOverrides);
    void bindInternalUniforms(const PassExecutionPlan& plan, const SequenceSystemUniformState& systemUniforms);
    void preparePassAtomicCounters(Pass& pass);
    void bindPassAtomicCounters(Pass& pass);
    void capturePassAtomicCounterResults(Pass& pass);
    void executeComputePass(const PassExecutionPlan& plan, int textureIndex);
    void executeGraphicsPass(const PassExecutionPlan& plan);
    void savePassOutputs(const PassExecutionPlan& plan);
    void destroyAtomicCounterBuffers();
    void initCommon();
    void initializeFromParsedArguments(const SequenceParsedArguments& parsedArguments);
    void initializeFromBuildConfig(const SequenceBuildConfig& buildConfig);
    void initializeFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig);
};

std::shared_ptr<SequenceSharedGpuMesh>
Sequence_CreateSharedGpuMesh(const SequenceSharedMeshData& sharedMesh);
