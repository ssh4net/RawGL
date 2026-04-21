// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022 Erium Vladlen.

#pragma once

#include "common.h"
#include "texture.h"
#include "program.h"

#include <cmath>
#include <cstring>
#include <functional>

namespace rawgl::io {
class IoRuntimeService;
}

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


struct SequencePass;

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

    static const std::vector<TexAttr> TEX_ATTR_ARR;

    std::map<std::string, std::string> attributes;
    GLProgramUniform* uniform;
    std::shared_ptr<Texture> texture;
    bool runtimeTextureBindingRequired = false;
    bool usesArrayElement = false;
    GLint addressedLocation = -1;
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
    int channels;
    int alphaChannel;

    GLProgramOutput* output;
    GLProgramUniform* uniform;
    std::shared_ptr<Texture> texture;
    bool usesArrayElement = false;
    size_t arrayElement = 0;

    PassOutput();
};

struct SequencePass {
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

    using BObject = GenericObject<GLuint>;

    struct GLBO {
        std::vector<FBOobject> FBO;
        std::vector<VAObject> VBO;
        std::map<std::string, BObject> BO;
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

    static const std::vector<SequencePass::CullModeAttr> CULL_PARM_ARR;

    const void eval_cull_parm(hres& hr, const std::string& name, const std::string& attr_val_name);

    SequencePass(const std::shared_ptr<GLProgram>& p, bool isCompute)
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
_pass_input_set_cull_face(SequencePass::CullMode& mm, const GLuint& val);
const void
_pass_input_set_wind_order(SequencePass::CullMode& mm, const GLuint& val);
const void
_pass_input_set_cull_enable(SequencePass::CullMode& mm, const GLuint& val);

struct SequenceRuntimePassConfig {
    std::shared_ptr<GLProgram> program;
    bool isCompute = false;
    std::map<std::string, PassInput> inputs;
    std::map<std::string, SequencePass::inputCounter> inputCounters;
    std::map<std::string, PassOutput> outputs;
    std::map<std::string, MeshInput> meshes;
    SequencePass::CullMode cullMode { GL_CW, GL_BACK, true };
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
    std::shared_ptr<rawgl::io::IoRuntimeService> ioRuntime;
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
    bool usesArrayElement = false;
    size_t arrayElement = 0;
};

class Sequence {
public:
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
    struct PlannedInputBinding {
        const std::string* name = nullptr;
        PassInput* input        = nullptr;
    };

    struct PlannedOutputBinding {
        PassOutput* output = nullptr;
    };

    struct PassExecutionPlan {
        SequencePass* pass              = nullptr;
        const MeshInput* primaryMesh    = nullptr;
        int passIndex                   = -1;
        std::vector<PlannedInputBinding> inputs;
        std::vector<PlannedOutputBinding> outputs;
    };

    std::map<std::string, std::shared_ptr<Texture>> m_textures;
    std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> m_sharedMeshes;
    std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> m_sharedGpuMeshes;
    std::shared_ptr<rawgl::io::IoRuntimeService> m_ioRuntime;

    std::vector<SequencePass> m_passes;
    std::vector<PassExecutionPlan> m_executionPlan;
    bool m_runTexturesDirty = false;

    void buildPassesFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig);
    void preloadInputTextures();
    void ensurePassOutputTextures(SequencePass& pass, int passIndex);
    void refreshPassTextureInputs(SequencePass& pass);
    void prepareRunTextures();
    void initializePass(SequencePass& pass, int passIndex);
    void validatePassSetup() const;
    void buildExecutionPlan();
    int bindPassInputs(const PassExecutionPlan& plan,
                       const std::vector<SequenceExecutionInputOverride>& inputOverrides);
    void bindInternalUniforms(const PassExecutionPlan& plan, const SequenceSystemUniformState& systemUniforms);
    void initializePassAtomicCounters(SequencePass& pass);
    void preparePassAtomicCounters(SequencePass& pass);
    void bindPassAtomicCounters(SequencePass& pass);
    void capturePassAtomicCounterResults(SequencePass& pass);
    void executeComputePass(const PassExecutionPlan& plan, int textureIndex);
    void executeGraphicsPass(const PassExecutionPlan& plan);
    void destroyAtomicCounterBuffers();
    void initCommon();
    void initializeFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig);
};

std::shared_ptr<SequenceSharedGpuMesh>
Sequence_CreateSharedGpuMesh(const SequenceSharedMeshData& sharedMesh);
