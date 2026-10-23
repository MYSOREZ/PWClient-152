#pragma once

// d3d9gles: a Direct3D 9 shaped device implemented on OpenGL ES 3.0.
//
// The engine calls D3D-style methods; this class caches state, translates it to GL, and draws.
// It is deliberately not COM: no refcounts, no interfaces, no vtable games. Angelica3D owns the
// objects, so ownership stays with the engine.

#include <GLES3/gl3.h>

#include <cstdint>
#include <unordered_map>

#include "pwx_d3dtypes.h"

namespace pwx::gles {

// Row-major storage, row-vector convention: v' = v * M. Same as D3D9.
struct Matrix {
    float m[16];
};

Matrix Identity();
Matrix Multiply(const Matrix& a, const Matrix& b);
Matrix Transpose(const Matrix& a);
Matrix Translation(float x, float y, float z);
Matrix RotationY(float radians);
Matrix PerspectiveFovLH(float fovYRadians, float aspect, float zNear, float zFar);
Matrix LookAtLH(const float eye[3], const float at[3], const float up[3]);

class Device {
public:
    bool Init();
    void Shutdown();

    void SetBackBufferSize(int width, int height);
    void Clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil);
    void SetViewport(int x, int y, int width, int height, float minZ, float maxZ);

    void SetRenderState(uint32_t state, uint32_t value);
    void SetSamplerState(uint32_t stage, uint32_t type, uint32_t value);
    void SetTexture(uint32_t stage, GLuint texture);
    void SetTransform(uint32_t state, const Matrix& matrix);
    void SetStreamSource(uint32_t stream, GLuint buffer, uint32_t offset, uint32_t stride);
    void SetIndices(GLuint buffer);
    void SetFVF(uint32_t fvf);

    void DrawIndexedPrimitive(uint32_t type, int32_t baseVertex, uint32_t minIndex,
                              uint32_t numVertices, uint32_t startIndex, uint32_t primCount);
    void Present();

    uint32_t drawCalls() const { return draws_; }
    uint32_t triangles() const { return triangles_; }

private:
    struct Layout {
        bool valid = false;
        bool hasDiffuse = false;
        bool hasNormal = false;
        uint32_t texCount = 0;
        uint32_t stride = 0;
        uint32_t positionOffset = 0;
        uint32_t diffuseOffset = 0;
        uint32_t normalOffset = 0;
        uint32_t texOffset[4] = {0, 0, 0, 0};
    };

    static Layout DecodeFvf(uint32_t fvf, uint32_t stride);
    GLuint GetOrCreateVao(uint64_t key, const Layout& layout, uint32_t baseBytes);
    void ApplyRenderState(uint32_t state, uint32_t value);
    void ApplySamplerState(uint32_t stage, uint32_t type, uint32_t value);
    void ApplyTransforms();
    void EnsureProgram();

    GLuint program_ = 0;
    GLint uMvp_ = -1;
    GLint uTexture_ = -1;
    GLint uUseTexture_ = -1;
    GLint uTextureFactor_ = -1;
    GLint uAlphaFunc_ = -1;
    GLint uAlphaRef_ = -1;

    GLuint vao_ = 0;
    std::unordered_map<uint64_t, GLuint> vaoCache_;

    GLuint stream_[2] = {0, 0};
    uint32_t streamOffset_[2] = {0, 0};
    uint32_t streamStride_[2] = {0, 0};
    GLuint indices_ = 0;
    uint32_t fvf_ = 0;

    GLuint textures_[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t samplerState_[8][16] = {};

    uint32_t renderState_[256] = {};
    bool renderStateApplied_[256] = {};

    Matrix world_ = Identity();
    Matrix view_ = Identity();
    Matrix projection_ = Identity();

    int backBufferWidth_ = 0;
    int backBufferHeight_ = 0;
    uint32_t draws_ = 0;
    uint32_t triangles_ = 0;
    bool loggedUnsupported_ = false;
};

}  // namespace pwx::gles
