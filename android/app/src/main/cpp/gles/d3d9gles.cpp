#include "d3d9gles.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "pwx_log.h"

namespace pwx::gles {
namespace {

constexpr float kPi = 3.14159265358979323846f;

GLenum BlendFactor(uint32_t value) {
    // D3DBLEND and GL blend factors share the same order for the supported range.
    static const GLenum kTable[] = {
        GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
        GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA,
        GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA_SATURATE,
    };
    if (value < d3d::D3DBLEND_ZERO || value > d3d::D3DBLEND_SRCALPHASAT) {
        return GL_ONE;
    }
    return kTable[value - d3d::D3DBLEND_ZERO];
}

GLenum CompareFunc(uint32_t value) {
    switch (value) {
        case d3d::D3DCMP_NEVER: return GL_NEVER;
        case d3d::D3DCMP_LESS: return GL_LESS;
        case d3d::D3DCMP_EQUAL: return GL_EQUAL;
        case d3d::D3DCMP_LESSEQUAL: return GL_LEQUAL;
        case d3d::D3DCMP_GREATER: return GL_GREATER;
        case d3d::D3DCMP_NOTEQUAL: return GL_NOTEQUAL;
        case d3d::D3DCMP_GREATEREQUAL: return GL_GEQUAL;
        case d3d::D3DCMP_ALWAYS: return GL_ALWAYS;
        default: return GL_LEQUAL;
    }
}

GLenum AddressMode(uint32_t value) {
    switch (value) {
        case d3d::D3DTADDRESS_WRAP: return GL_REPEAT;
        case d3d::D3DTADDRESS_MIRROR: return GL_MIRRORED_REPEAT;
        case d3d::D3DTADDRESS_CLAMP: return GL_CLAMP_TO_EDGE;
        case d3d::D3DTADDRESS_MIRRORONCE: return GL_MIRRORED_REPEAT;
        default: return GL_REPEAT;  // D3D default is WRAP
    }
}

GLenum MinFilter(uint32_t filter, uint32_t mip) {
    switch (mip) {
        case d3d::D3DTEXF_POINT:
            return filter == d3d::D3DTEXF_POINT ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
        case d3d::D3DTEXF_LINEAR:
            return filter == d3d::D3DTEXF_POINT ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
        default:
            return filter == d3d::D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR;
    }
}

GLenum MagFilter(uint32_t filter) {
    return filter == d3d::D3DTEXF_POINT ? GL_NEAREST : GL_LINEAR;
}

GLenum PrimitiveMode(uint32_t type, uint32_t& indexCountFactor) {
    switch (type) {
        case d3d::D3DPT_POINTLIST: indexCountFactor = 1; return GL_POINTS;
        case d3d::D3DPT_LINELIST: indexCountFactor = 2; return GL_LINES;
        case d3d::D3DPT_LINESTRIP: indexCountFactor = 1; return GL_LINE_STRIP;
        case d3d::D3DPT_TRIANGLELIST: indexCountFactor = 3; return GL_TRIANGLES;
        case d3d::D3DPT_TRIANGLESTRIP: indexCountFactor = 1; return GL_TRIANGLE_STRIP;
        case d3d::D3DPT_TRIANGLEFAN: indexCountFactor = 1; return GL_TRIANGLE_FAN;
        default: indexCountFactor = 3; return GL_TRIANGLES;
    }
}

const char* kVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aDiffuse;
layout(location = 2) in vec2 aUV;
uniform mat4 uMVP;
out vec4 vColor;
out vec2 vUV;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    // D3DCOLOR is 0xAARRGGBB; little-endian bytes arrive as B,G,R,A.
    vColor = aDiffuse.bgra;
    // D3D texture space has the origin in the top-left corner, GL in the bottom-left.
    vUV = vec2(aUV.x, 1.0 - aUV.y);
}
)";

const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec4 vColor;
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uUseTexture;
uniform vec4 uTextureFactor;
uniform int uAlphaFunc;
uniform float uAlphaRef;
out vec4 oColor;
void main() {
    vec4 c = vColor * uTextureFactor;
    if (uUseTexture > 0.5) {
        c *= texture(uTexture, vUV);
    }
    if (uAlphaFunc > 0) {
        bool pass = true;
        if (uAlphaFunc == 1) pass = false;
        else if (uAlphaFunc == 2) pass = c.a < uAlphaRef;
        else if (uAlphaFunc == 3) pass = abs(c.a - uAlphaRef) < 0.001;
        else if (uAlphaFunc == 4) pass = c.a <= uAlphaRef;
        else if (uAlphaFunc == 5) pass = c.a > uAlphaRef;
        else if (uAlphaFunc == 6) pass = abs(c.a - uAlphaRef) >= 0.001;
        else if (uAlphaFunc == 7) pass = c.a >= uAlphaRef;
        if (!pass) discard;
    }
    oColor = c;
}
)";

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLchar info[1024] = {0};
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        PWX_LOGE("d3d9gles", "%s shader: %s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

Matrix Identity() {
    Matrix r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Matrix Multiply(const Matrix& a, const Matrix& b) {
    Matrix r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[row * 4 + k] * b.m[k * 4 + col];
            }
            r.m[row * 4 + col] = sum;
        }
    }
    return r;
}

Matrix Transpose(const Matrix& a) {
    Matrix r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[col * 4 + row] = a.m[row * 4 + col];
        }
    }
    return r;
}

Matrix Translation(float x, float y, float z) {
    Matrix r = Identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

Matrix RotationY(float radians) {
    Matrix r = Identity();
    const float c = cosf(radians);
    const float s = sinf(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

Matrix PerspectiveFovLH(float fovYRadians, float aspect, float zNear, float zFar) {
    Matrix r{};
    const float h = 1.0f / tanf(fovYRadians * 0.5f);
    const float w = h / aspect;
    r.m[0] = w;
    r.m[5] = h;
    r.m[10] = zFar / (zFar - zNear);
    r.m[11] = 1.0f;
    r.m[14] = -zNear * zFar / (zFar - zNear);
    return r;
}

Matrix LookAtLH(const float eye[3], const float at[3], const float up[3]) {
    float zAxis[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
    float zLen = sqrtf(zAxis[0] * zAxis[0] + zAxis[1] * zAxis[1] + zAxis[2] * zAxis[2]);
    if (zLen < 1e-6f) {
        return Identity();
    }
    for (float& v : zAxis) {
        v /= zLen;
    }
    float xAxis[3] = {
        up[1] * zAxis[2] - up[2] * zAxis[1],
        up[2] * zAxis[0] - up[0] * zAxis[2],
        up[0] * zAxis[1] - up[1] * zAxis[0],
    };
    float xLen = sqrtf(xAxis[0] * xAxis[0] + xAxis[1] * xAxis[1] + xAxis[2] * xAxis[2]);
    if (xLen < 1e-6f) {
        return Identity();
    }
    for (float& v : xAxis) {
        v /= xLen;
    }
    const float yAxis[3] = {
        zAxis[1] * xAxis[2] - zAxis[2] * xAxis[1],
        zAxis[2] * xAxis[0] - zAxis[0] * xAxis[2],
        zAxis[0] * xAxis[1] - zAxis[1] * xAxis[0],
    };

    Matrix r = Identity();
    r.m[0] = xAxis[0]; r.m[1] = yAxis[0]; r.m[2] = zAxis[0];
    r.m[4] = xAxis[1]; r.m[5] = yAxis[1]; r.m[6] = zAxis[1];
    r.m[8] = xAxis[2]; r.m[9] = yAxis[2]; r.m[10] = zAxis[2];
    r.m[12] = -(xAxis[0] * eye[0] + xAxis[1] * eye[1] + xAxis[2] * eye[2]);
    r.m[13] = -(yAxis[0] * eye[0] + yAxis[1] * eye[1] + yAxis[2] * eye[2]);
    r.m[14] = -(zAxis[0] * eye[0] + zAxis[1] * eye[1] + zAxis[2] * eye[2]);
    return r;
}

bool Device::Init() {
    EnsureProgram();
    if (!program_) {
        return false;
    }

    // D3D9 defaults: documented state is applied up front so the engine can rely on it.
    renderState_[d3d::D3DRS_ZENABLE] = 1;
    renderState_[d3d::D3DRS_ZWRITEENABLE] = 1;
    renderState_[d3d::D3DRS_ZFUNC] = d3d::D3DCMP_LESSEQUAL;
    renderState_[d3d::D3DRS_CULLMODE] = d3d::D3DCULL_CCW;
    renderState_[d3d::D3DRS_SRCBLEND] = d3d::D3DBLEND_ONE;
    renderState_[d3d::D3DRS_DESTBLEND] = d3d::D3DBLEND_ZERO;
    renderState_[d3d::D3DRS_ALPHABLENDENABLE] = 0;
    renderState_[d3d::D3DRS_ALPHATESTENABLE] = 0;
    renderState_[d3d::D3DRS_ALPHAFUNC] = d3d::D3DCMP_ALWAYS;
    renderState_[d3d::D3DRS_ALPHAREF] = 0;
    renderState_[d3d::D3DRS_COLORWRITEENABLE] = 0xF;
    renderState_[d3d::D3DRS_LIGHTING] = 1;
    renderState_[d3d::D3DRS_TEXTUREFACTOR] = 0xFFFFFFFFu;
    renderState_[d3d::D3DRS_BLENDOP] = d3d::D3DBLENDOP_ADD;
    renderState_[d3d::D3DRS_DITHERENABLE] = 0;
    renderState_[d3d::D3DRS_SPECULARENABLE] = 0;
    renderState_[d3d::D3DRS_SCISSORTESTENABLE] = 0;
    renderState_[d3d::D3DRS_FOGENABLE] = 0;
    renderState_[d3d::D3DRS_STENCILENABLE] = 0;

    for (uint32_t state : {d3d::D3DRS_ZENABLE, d3d::D3DRS_ZWRITEENABLE, d3d::D3DRS_ZFUNC,
                           d3d::D3DRS_CULLMODE, d3d::D3DRS_SRCBLEND, d3d::D3DRS_DESTBLEND,
                           d3d::D3DRS_ALPHABLENDENABLE, d3d::D3DRS_ALPHATESTENABLE,
                           d3d::D3DRS_ALPHAFUNC, d3d::D3DRS_ALPHAREF,
                           d3d::D3DRS_COLORWRITEENABLE, d3d::D3DRS_TEXTUREFACTOR,
                           d3d::D3DRS_BLENDOP, d3d::D3DRS_SCISSORTESTENABLE}) {
        ApplyRenderState(state, renderState_[state]);
        renderStateApplied_[state] = true;
    }

    for (uint32_t stage = 0; stage < 8; ++stage) {
        samplerState_[stage][d3d::D3DSAMP_ADDRESSU] = d3d::D3DTADDRESS_WRAP;
        samplerState_[stage][d3d::D3DSAMP_ADDRESSV] = d3d::D3DTADDRESS_WRAP;
        samplerState_[stage][d3d::D3DSAMP_MAGFILTER] = d3d::D3DTEXF_POINT;
        samplerState_[stage][d3d::D3DSAMP_MINFILTER] = d3d::D3DTEXF_POINT;
        samplerState_[stage][d3d::D3DSAMP_MIPFILTER] = d3d::D3DTEXF_NONE;
    }

    // D3D's front face is clockwise; GL's default is counter-clockwise.
    glFrontFace(GL_CW);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    PWX_LOGI("d3d9gles", "device ready: D3D9 defaults applied, GLES 3.0 state cache live");
    return true;
}

void Device::Shutdown() {
    for (auto& entry : vaoCache_) {
        glDeleteVertexArrays(1, &entry.second);
    }
    vaoCache_.clear();
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    PWX_LOGI("d3d9gles", "device shut down: %u draws, %u triangles", draws_, triangles_);
}

void Device::EnsureProgram() {
    if (program_) {
        return;
    }
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) {
        return;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLchar info[1024] = {0};
        glGetProgramInfoLog(program_, sizeof(info), nullptr, info);
        PWX_LOGE("d3d9gles", "link failed: %s", info);
        glDeleteProgram(program_);
        program_ = 0;
        return;
    }

    uMvp_ = glGetUniformLocation(program_, "uMVP");
    uTexture_ = glGetUniformLocation(program_, "uTexture");
    uUseTexture_ = glGetUniformLocation(program_, "uUseTexture");
    uTextureFactor_ = glGetUniformLocation(program_, "uTextureFactor");
    uAlphaFunc_ = glGetUniformLocation(program_, "uAlphaFunc");
    uAlphaRef_ = glGetUniformLocation(program_, "uAlphaRef");
}

void Device::SetBackBufferSize(int width, int height) {
    backBufferWidth_ = width;
    backBufferHeight_ = height;
}

void Device::Clear(uint32_t flags, uint32_t argb, float z, uint32_t stencil) {
    GLbitfield mask = 0;
    if (flags & 0x1) {
        mask |= GL_COLOR_BUFFER_BIT;
        glClearColor(((argb >> 16) & 0xFF) / 255.0f,
                     ((argb >> 8) & 0xFF) / 255.0f,
                     (argb & 0xFF) / 255.0f,
                     ((argb >> 24) & 0xFF) / 255.0f);
    }
    if (flags & 0x2) {
        mask |= GL_DEPTH_BUFFER_BIT;
        glDepthMask(GL_TRUE);
        glClearDepthf(z);
    }
    if (flags & 0x4) {
        mask |= GL_STENCIL_BUFFER_BIT;
        glClearStencil(static_cast<GLint>(stencil));
    }
    if (mask) {
        glClear(mask);
    }
}

void Device::SetViewport(int x, int y, int width, int height, float minZ, float maxZ) {
    // D3D viewport origin is top-left, GL is bottom-left.
    const int flipped = backBufferHeight_ > 0 ? backBufferHeight_ - y - height : y;
    glViewport(x, flipped, width, height);
    glDepthRangef(minZ, maxZ);
}

void Device::SetRenderState(uint32_t state, uint32_t value) {
    if (state >= 256) {
        return;
    }
    if (renderStateApplied_[state] && renderState_[state] == value) {
        return;
    }
    renderState_[state] = value;
    renderStateApplied_[state] = true;
    ApplyRenderState(state, value);
}

void Device::ApplyRenderState(uint32_t state, uint32_t value) {
    switch (state) {
        case d3d::D3DRS_ZENABLE:
            if (value) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            break;
        case d3d::D3DRS_ZWRITEENABLE:
            glDepthMask(value ? GL_TRUE : GL_FALSE);
            break;
        case d3d::D3DRS_ZFUNC:
            glDepthFunc(CompareFunc(value));
            break;
        case d3d::D3DRS_CULLMODE:
            switch (value) {
                case d3d::D3DCULL_NONE:
                    glDisable(GL_CULL_FACE);
                    break;
                case d3d::D3DCULL_CW:
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                    break;
                case d3d::D3DCULL_CCW:
                default:
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                    break;
            }
            break;
        case d3d::D3DRS_SRCBLEND:
            glBlendFunc(BlendFactor(value), BlendFactor(renderState_[d3d::D3DRS_DESTBLEND]));
            break;
        case d3d::D3DRS_DESTBLEND:
            glBlendFunc(BlendFactor(renderState_[d3d::D3DRS_SRCBLEND]), BlendFactor(value));
            break;
        case d3d::D3DRS_ALPHABLENDENABLE:
            if (value) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            break;
        case d3d::D3DRS_BLENDOP: {
            GLenum op = GL_FUNC_ADD;
            switch (value) {
                case d3d::D3DBLENDOP_SUBTRACT: op = GL_FUNC_SUBTRACT; break;
                case d3d::D3DBLENDOP_REVSUBTRACT: op = GL_FUNC_REVERSE_SUBTRACT; break;
                default: break;  // MIN/MAX need an extension hint; ADD covers the client
            }
            glBlendEquation(op);
            break;
        }
        case d3d::D3DRS_COLORWRITEENABLE:
            glColorMask((value & 0x1) ? GL_TRUE : GL_FALSE,
                        (value & 0x2) ? GL_TRUE : GL_FALSE,
                        (value & 0x4) ? GL_TRUE : GL_FALSE,
                        (value & 0x8) ? GL_TRUE : GL_FALSE);
            break;
        case d3d::D3DRS_SCISSORTESTENABLE:
            if (value) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
            break;
        case d3d::D3DRS_STENCILENABLE:
            if (value) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
            break;
        case d3d::D3DRS_DEPTHBIAS:
        case d3d::D3DRS_SLOPESCALEDEPTHBIAS:
            // Rarely used by the client; polygon offset needs both values together.
            break;
        // Read at draw time (texture factor, alpha test) or intentionally ignored until the
        // shader system exists (lighting, dither, specular, fog).
        case d3d::D3DRS_TEXTUREFACTOR:
        case d3d::D3DRS_ALPHATESTENABLE:
        case d3d::D3DRS_ALPHAFUNC:
        case d3d::D3DRS_ALPHAREF:
        case d3d::D3DRS_LIGHTING:
        case d3d::D3DRS_DITHERENABLE:
        case d3d::D3DRS_SPECULARENABLE:
        case d3d::D3DRS_FOGENABLE:
            break;
        default:
            if (!loggedUnsupported_) {
                loggedUnsupported_ = true;
                PWX_LOGW("d3d9gles", "render state %u = %u not mapped yet (logged once)",
                         state, value);
            }
            break;
    }
}

void Device::SetSamplerState(uint32_t stage, uint32_t type, uint32_t value) {
    if (stage >= 8 || type >= 16) {
        return;
    }
    samplerState_[stage][type] = value;
    if (textures_[stage]) {
        ApplySamplerState(stage, type, value);
    }
}

void Device::ApplySamplerState(uint32_t stage, uint32_t type, uint32_t value) {
    glActiveTexture(GL_TEXTURE0 + stage);
    glBindTexture(GL_TEXTURE_2D, textures_[stage]);
    switch (type) {
        case d3d::D3DSAMP_ADDRESSU:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, AddressMode(value));
            break;
        case d3d::D3DSAMP_ADDRESSV:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, AddressMode(value));
            break;
        case d3d::D3DSAMP_MINFILTER:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            MinFilter(value, samplerState_[stage][d3d::D3DSAMP_MIPFILTER]));
            break;
        case d3d::D3DSAMP_MAGFILTER:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, MagFilter(value));
            break;
        case d3d::D3DSAMP_MIPFILTER:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            MinFilter(samplerState_[stage][d3d::D3DSAMP_MINFILTER], value));
            break;
        default:
            break;
    }
}

void Device::SetTexture(uint32_t stage, GLuint texture) {
    if (stage >= 8) {
        return;
    }
    textures_[stage] = texture;
    glActiveTexture(GL_TEXTURE0 + stage);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (texture) {
        for (uint32_t type = 1; type < 8; ++type) {
            if (samplerState_[stage][type]) {
                ApplySamplerState(stage, type, samplerState_[stage][type]);
            }
        }
    }
}

void Device::SetTransform(uint32_t state, const Matrix& matrix) {
    switch (state) {
        case d3d::D3DTS_WORLD: world_ = matrix; break;
        case d3d::D3DTS_VIEW: view_ = matrix; break;
        case d3d::D3DTS_PROJECTION: projection_ = matrix; break;
        default: break;
    }
}

void Device::SetStreamSource(uint32_t stream, GLuint buffer, uint32_t offset, uint32_t stride) {
    if (stream >= 2) {
        return;
    }
    stream_[stream] = buffer;
    streamOffset_[stream] = offset;
    streamStride_[stream] = stride;
}

void Device::SetIndices(GLuint buffer) {
    indices_ = buffer;
}

void Device::SetFVF(uint32_t fvf) {
    fvf_ = fvf;
}

Device::Layout Device::DecodeFvf(uint32_t fvf, uint32_t stride) {
    Layout layout;
    uint32_t offset = 0;
    const uint32_t position = fvf & d3d::D3DFVF_POSITION_MASK;
    if (position == d3d::D3DFVF_XYZ) {
        layout.positionOffset = offset;
        offset += 3 * sizeof(float);
    } else if (position == d3d::D3DFVF_XYZRHW) {
        layout.positionOffset = offset;
        offset += 4 * sizeof(float);
    } else {
        PWX_LOGE("d3d9gles", "FVF 0x%08x: unsupported position type", fvf);
        return layout;
    }
    if (fvf & d3d::D3DFVF_NORMAL) {
        layout.hasNormal = true;
        layout.normalOffset = offset;
        offset += 3 * sizeof(float);
    }
    if (fvf & d3d::D3DFVF_DIFFUSE) {
        layout.hasDiffuse = true;
        layout.diffuseOffset = offset;
        offset += sizeof(uint32_t);
    }
    if (fvf & d3d::D3DFVF_SPECULAR) {
        offset += sizeof(uint32_t);  // accepted, not consumed by the demo shader
    }
    layout.texCount = (fvf & d3d::D3DFVF_TEXCOUNT_MASK) >> 8;
    for (uint32_t i = 0; i < layout.texCount && i < 4; ++i) {
        layout.texOffset[i] = offset;
        offset += 2 * sizeof(float);  // 2D coordinates; 1D/3D/4D rare in this client
    }
    layout.stride = stride ? stride : offset;
    layout.valid = true;
    return layout;
}

GLuint Device::GetOrCreateVao(uint64_t key, const Layout& layout, uint32_t baseBytes) {
    auto it = vaoCache_.find(key);
    if (it != vaoCache_.end()) {
        return it->second;
    }
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, stream_[0]);
    const uint32_t base = streamOffset_[0] + baseBytes;
    const uint32_t stride = layout.stride;

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(static_cast<uintptr_t>(base + layout.positionOffset)));
    if (layout.hasDiffuse) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                              reinterpret_cast<const void*>(static_cast<uintptr_t>(base + layout.diffuseOffset)));
    }
    if (layout.texCount > 0) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(static_cast<uintptr_t>(base + layout.texOffset[0])));
    }
    glBindVertexArray(0);

    vaoCache_[key] = vao;
    PWX_LOGI("d3d9gles", "vao %u created: vbo=%u base=%u stride=%u posOff=%u difOff=%u uvOff=%u texCount=%u",
             vao, stream_[0], base, stride, layout.positionOffset, layout.diffuseOffset,
             layout.texOffset[0], layout.texCount);
    return vao;
}

void Device::ApplyTransforms() {
    // D3D clip space keeps z in [0, w]; GL expects [-w, w]. Remap after projection.
    Matrix toGl = Identity();
    toGl.m[10] = 2.0f;
    toGl.m[14] = -1.0f;

    const Matrix mvp = Multiply(Multiply(Multiply(world_, view_), projection_), toGl);
    // The shader multiplies uMVP * v in GL column-vector order, so the data must
    // arrive as mvp^T. Row-major mvp bytes with GL transpose=GL_FALSE are read by GL
    // as column-major, which lands as exactly mvp^T - so upload mvp.m directly.
    // (Do NOT pre-transpose: that double-transposes and the GPU ends up applying
    //  the matrix with swapped sides - the cube blew up to a screen-filling quad.)
    glUniformMatrix4fv(uMvp_, 1, GL_FALSE, mvp.m);
}

void Device::DrawIndexedPrimitive(uint32_t type, int32_t baseVertex, uint32_t minIndex,
                                  uint32_t numVertices, uint32_t startIndex, uint32_t primCount) {
    (void)minIndex;
    (void)numVertices;
    if (!program_ || !indices_ || !stream_[0] || !fvf_ || !primCount) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            PWX_LOGW("d3d9gles", "draw skipped: program=%u indices=%u vb=%u fvf=0x%x prims=%u",
                     program_, indices_, stream_[0], fvf_, primCount);
        }
        return;
    }

    const Layout layout = DecodeFvf(fvf_, streamStride_[0]);
    if (!layout.valid) {
        return;
    }

    // baseVertex is emulated by shifting the buffer origin: every attribute moves together.
    const uint32_t baseBytes = baseVertex > 0
            ? static_cast<uint32_t>(baseVertex) * layout.stride
            : 0u;
    uint64_t key = static_cast<uint64_t>(stream_[0]) | 0x100000000ull;
    key = key * 1099511628211ull ^ (static_cast<uint64_t>(fvf_) + 0x100ull);
    key = key * 1099511628211ull ^ (static_cast<uint64_t>(layout.stride) + 0x100ull);
    key = key * 1099511628211ull ^ (static_cast<uint64_t>(baseBytes) + 1ull);
    const GLuint vao = GetOrCreateVao(key, layout, baseBytes);
    if (!vao) {
        return;
    }
    uint32_t factor = 3;
    const GLenum mode = PrimitiveMode(type, factor);
    uint32_t count = primCount * factor;
    if (type == d3d::D3DPT_TRIANGLESTRIP || type == d3d::D3DPT_TRIANGLEFAN ||
        type == d3d::D3DPT_LINESTRIP) {
        count = primCount + 2;
    } else if (type == d3d::D3DPT_LINELIST) {
        count = primCount * 2;
    }

    glUseProgram(program_);
    ApplyTransforms();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[0]);
    glUniform1i(uTexture_, 0);
    glUniform1f(uUseTexture_, textures_[0] ? 1.0f : 0.0f);

    const uint32_t tf = renderState_[d3d::D3DRS_TEXTUREFACTOR];
    glUniform4f(uTextureFactor_,
                ((tf >> 16) & 0xFF) / 255.0f,
                ((tf >> 8) & 0xFF) / 255.0f,
                (tf & 0xFF) / 255.0f,
                ((tf >> 24) & 0xFF) / 255.0f);

    const bool alphaTest = renderState_[d3d::D3DRS_ALPHATESTENABLE] != 0;
    glUniform1i(uAlphaFunc_, alphaTest ? static_cast<GLint>(renderState_[d3d::D3DRS_ALPHAFUNC]) : 0);
    glUniform1f(uAlphaRef_, (renderState_[d3d::D3DRS_ALPHAREF] & 0xFF) / 255.0f);

    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);

    glDrawElements(mode, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT,
                   reinterpret_cast<const void*>(static_cast<uintptr_t>(startIndex * sizeof(uint16_t))));
    glBindVertexArray(0);

    draws_++;
    if (mode == GL_TRIANGLES) {
        triangles_ += count / 3;
    }

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            PWX_LOGE("d3d9gles", "GL error 0x%04x after draw", err);
        }
    }

}

void Device::Present() {
    // The swap chain belongs to SDL (SDL_GL_SwapWindow); nothing to do here yet.
}

}  // namespace pwx::gles
