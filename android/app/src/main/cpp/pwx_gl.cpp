#include "pwx_gl.h"

#include <GLES3/gl3.h>

#include <cmath>

#include "pwx_log.h"

namespace pwx {
namespace {

GLuint g_program = 0;
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLint g_uTime = -1;
bool g_ready = false;

const char* kVertexShader = R"(#version 300 es
in vec2 aPos;
in vec3 aColor;
uniform float uTime;
out vec3 vColor;
void main() {
    float s = sin(uTime);
    float c = cos(uTime);
    mat2 rot = mat2(c, -s, s, c);
    gl_Position = vec4(rot * aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

const char* kFragmentShader = R"(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 oColor;
void main() {
    oColor = vec4(vColor, 1.0);
}
)";

GLuint Compile(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLchar info[1024] = {0};
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        PWX_LOGE("GL", "%s shader failed: %s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void DumpGlInfo() {
    PWX_LOGI("GL", "vendor: %s", glGetString(GL_VENDOR));
    PWX_LOGI("GL", "renderer: %s", glGetString(GL_RENDERER));
    PWX_LOGI("GL", "version: %s", glGetString(GL_VERSION));
    PWX_LOGI("GL", "glsl: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    GLint maxTexture = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    PWX_LOGI("GL", "max texture size: %d", maxTexture);
}

void LogGlError(const char* where) {
    for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
        PWX_LOGE("GL", "%s: GL error 0x%04x", where, err);
    }
}

}  // namespace

void GlInit() {
    DumpGlInfo();

    GLuint vs = Compile(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = Compile(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) {
        return;
    }

    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glBindAttribLocation(g_program, 0, "aPos");
    glBindAttribLocation(g_program, 1, "aColor");
    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = GL_FALSE;
    glGetProgramiv(g_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLchar info[1024] = {0};
        glGetProgramInfoLog(g_program, sizeof(info), nullptr, info);
        PWX_LOGE("GL", "program link failed: %s", info);
        return;
    }

    g_uTime = glGetUniformLocation(g_program, "uTime");

    // Triangle in clip space with per-vertex colour: 2 floats position + 3 floats colour.
    const float vertices[] = {
        0.00f, 0.60f, 1.0f, 0.35f, 0.25f,
        -0.55f, -0.45f, 0.25f, 1.0f, 0.45f,
        0.55f, -0.45f, 0.35f, 0.55f, 1.0f,
    };

    glGenVertexArrays(1, &g_vao);
    glBindVertexArray(g_vao);

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    const GLsizei stride = 5 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void*>(2 * sizeof(float)));

    glBindVertexArray(0);

    LogGlError("init");
    g_ready = true;
    PWX_LOGI("GL", "pipeline ready (program=%u vao=%u)", g_program, g_vao);
}

void GlRender(float timeSeconds) {
    glClearColor(0.06f, 0.07f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!g_ready) {
        return;
    }

    glUseProgram(g_program);
    glUniform1f(g_uTime, timeSeconds);
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    static bool firstErrorLogged = false;
    if (!firstErrorLogged) {
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            PWX_LOGE("GL", "first draw error 0x%04x", err);
            firstErrorLogged = true;
        }
    }
}

}  // namespace pwx
