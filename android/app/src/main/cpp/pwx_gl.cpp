#include "pwx_gl.h"

#include <GLES3/gl3.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gles/d3d9gles.h"
#include "gles/pwx_d3dtypes.h"
#include "pwx_log.h"

namespace pwx {
namespace {

using gles::Device;

// XYZ | DIFFUSE | TEX1, 24 bytes. D3DCOLOR is stored 0xAARRGGBB and read as BGRA bytes.
struct Vertex {
    float x, y, z;
    uint32_t color;
    float u, v;
};

Device g_device;
GLuint g_vertexBuffer = 0;
GLuint g_indexBuffer = 0;
GLuint g_texture = 0;
bool g_ready = false;

uint32_t Color(uint32_t a, uint32_t r, uint32_t g, uint32_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void AddFace(std::vector<Vertex>& vertices, std::vector<uint16_t>& indices,
             const float (&origin)[3], const float (&du)[3], const float (&dv)[3],
             uint32_t color) {
    const uint16_t base = static_cast<uint16_t>(vertices.size());
    for (int corner = 0; corner < 4; ++corner) {
        const float u = (corner == 1 || corner == 2) ? 1.0f : 0.0f;
        const float v = (corner >= 2) ? 1.0f : 0.0f;
        Vertex vertex{};
        vertex.x = origin[0] + du[0] * u + dv[0] * v;
        vertex.y = origin[1] + du[1] * u + dv[1] * v;
        vertex.z = origin[2] + du[2] * u + dv[2] * v;
        vertex.color = color;
        vertex.u = u;
        vertex.v = v;
        vertices.push_back(vertex);
    }
    indices.insert(indices.end(), {base, static_cast<uint16_t>(base + 1),
                                   static_cast<uint16_t>(base + 2), base,
                                   static_cast<uint16_t>(base + 2),
                                   static_cast<uint16_t>(base + 3)});
}

void BuildCube() {
    const float s = 0.8f;
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    vertices.reserve(24);
    indices.reserve(36);

    // Faces are listed so that the winding order matches D3D's clockwise front face.
    AddFace(vertices, indices, {-s, -s, -s}, {2 * s, 0, 0}, {0, 2 * s, 0}, Color(255, 220, 90, 70));   // front (-Z)
    AddFace(vertices, indices, {s, -s, s}, {-2 * s, 0, 0}, {0, 2 * s, 0}, Color(255, 70, 180, 220));    // back (+Z)
    AddFace(vertices, indices, {-s, -s, s}, {0, 0, -2 * s}, {0, 2 * s, 0}, Color(255, 110, 220, 110));  // left (-X)
    AddFace(vertices, indices, {s, -s, -s}, {0, 0, 2 * s}, {0, 2 * s, 0}, Color(255, 220, 210, 100));   // right (+X)
    AddFace(vertices, indices, {-s, s, -s}, {2 * s, 0, 0}, {0, 0, 2 * s}, Color(255, 240, 240, 240));   // top (+Y)
    AddFace(vertices, indices, {-s, -s, s}, {2 * s, 0, 0}, {0, 0, -2 * s}, Color(255, 90, 90, 110));    // bottom (-Y)

    glGenBuffers(1, &g_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, g_vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &g_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint16_t)),
                 indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    PWX_LOGI("scene", "cube: %zu vertices, %zu indices", vertices.size(), indices.size());
}

void BuildCheckerTexture() {
    constexpr int kSize = 64;
    constexpr int kCell = 8;
    std::vector<uint8_t> pixels(kSize * kSize * 4);
    const uint8_t kLight[3] = {235, 235, 235};
    const uint8_t kDark[3] = {40, 40, 48};
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const bool light = ((x / kCell) + (y / kCell)) % 2 == 0;
            const uint8_t* rgb = light ? kLight : kDark;
            uint8_t* dst = &pixels[(y * kSize + x) * 4];
            dst[0] = rgb[0];
            dst[1] = rgb[1];
            dst[2] = rgb[2];
            dst[3] = 255;
        }
    }
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    PWX_LOGI("scene", "checker texture %dx%d created", kSize, kSize);
}

}  // namespace

void GlInit() {
    if (!g_device.Init()) {
        PWX_LOGE("scene", "device init failed");
        return;
    }

    BuildCube();
    BuildCheckerTexture();

    // Sampler states are cached by the device and applied when the texture is bound.
    g_device.SetSamplerState(0, d3d::D3DSAMP_MINFILTER, d3d::D3DTEXF_LINEAR);
    g_device.SetSamplerState(0, d3d::D3DSAMP_MAGFILTER, d3d::D3DTEXF_LINEAR);
    g_device.SetSamplerState(0, d3d::D3DSAMP_MIPFILTER, d3d::D3DTEXF_LINEAR);
    g_device.SetSamplerState(0, d3d::D3DSAMP_ADDRESSU, d3d::D3DTADDRESS_WRAP);
    g_device.SetSamplerState(0, d3d::D3DSAMP_ADDRESSV, d3d::D3DTADDRESS_WRAP);

    g_device.SetRenderState(d3d::D3DRS_ZENABLE, 1);
    g_device.SetRenderState(d3d::D3DRS_ZWRITEENABLE, 1);
    g_device.SetRenderState(d3d::D3DRS_ZFUNC, d3d::D3DCMP_LESSEQUAL);
    // Culling stays off in the demo until the winding order is confirmed on device.
    g_device.SetRenderState(d3d::D3DRS_CULLMODE, d3d::D3DCULL_NONE);
    g_device.SetRenderState(d3d::D3DRS_ALPHABLENDENABLE, 0);

    g_device.SetFVF(d3d::D3DFVF_XYZ | d3d::D3DFVF_DIFFUSE | d3d::D3DFVF_TEX1);
    g_device.SetStreamSource(0, g_vertexBuffer, 0, sizeof(Vertex));
    g_device.SetIndices(g_indexBuffer);
    g_device.SetTexture(0, g_texture);

    g_ready = true;
    PWX_LOGI("scene", "d3d9gles scene ready");
}

void GlRender(float timeSeconds, int width, int height) {
    if (!g_ready || width <= 0 || height <= 0) {
        return;
    }

    g_device.SetBackBufferSize(width, height);
    g_device.Clear(0x1 | 0x2, 0xFF101018u, 1.0f, 0);
    g_device.SetViewport(0, 0, width, height, 0.0f, 1.0f);

    const float eye[3] = {0.0f, 0.0f, -3.2f};
    const float at[3] = {0.0f, 0.0f, 0.0f};
    const float up[3] = {0.0f, 1.0f, 0.0f};

    g_device.SetTransform(d3d::D3DTS_WORLD, gles::RotationY(timeSeconds * 0.9f));
    g_device.SetTransform(d3d::D3DTS_VIEW, gles::LookAtLH(eye, at, up));
    g_device.SetTransform(d3d::D3DTS_PROJECTION,
                          gles::PerspectiveFovLH(60.0f * 3.14159265f / 180.0f,
                                                 static_cast<float>(width) / static_cast<float>(height),
                                                 0.1f, 100.0f));

    g_device.DrawIndexedPrimitive(d3d::D3DPT_TRIANGLELIST, 0, 0, 24, 0, 12);
}

void GlShutdown() {
    if (g_texture) {
        glDeleteTextures(1, &g_texture);
        g_texture = 0;
    }
    if (g_vertexBuffer) {
        glDeleteBuffers(1, &g_vertexBuffer);
        g_vertexBuffer = 0;
    }
    if (g_indexBuffer) {
        glDeleteBuffers(1, &g_indexBuffer);
        g_indexBuffer = 0;
    }
    g_device.Shutdown();
    g_ready = false;
}

}  // namespace pwx
