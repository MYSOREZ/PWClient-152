#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_version.h>

#include <jni.h>

#include <string>

#include "pwx_gl.h"
#include "pwx_log.h"

namespace {

void LogSdlVersion() {
    // SDL3: SDL_GetVersion() returns the runtime version as a packed integer.
    const int linked = SDL_GetVersion();
    PWX_LOGI("SDL", "runtime %d.%d.%d (headers %d.%d.%d)",
             SDL_VERSIONNUM_MAJOR(linked), SDL_VERSIONNUM_MINOR(linked),
             SDL_VERSIONNUM_MICRO(linked),
             SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
}

}  // namespace

// Called from Java once the activity knows where the log file lives.
// Must stay at global scope with C linkage: the JVM looks the symbol up by name.
extern "C" JNIEXPORT void JNICALL
Java_com_pwx_client_NativeBridge_setLogFile(JNIEnv* env, jclass, jstring path) {
    if (!path) {
        return;
    }
    const char* utf = env->GetStringUTFChars(path, nullptr);
    pwx::SetLogFile(utf);
    env->ReleaseStringUTFChars(path, utf);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    LogSdlVersion();
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        PWX_LOGE("SDL", "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // GLES 3.0 core: the whole port targets it, so ask for exactly that.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow("PWX", 0, 0,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        PWX_LOGE("SDL", "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        PWX_LOGE("SDL", "SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    PWX_LOGI("SDL", "window ready: %dx%d, swap interval 1", width, height);

    pwx::GlInit();

    bool running = true;
    Uint64 lastLog = SDL_GetTicks();
    Uint64 start = lastLog;
    int frames = 0;
    int touches = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    PWX_LOGI("SDL", "quit requested");
                    running = false;
                    break;
                case SDL_EVENT_FINGER_DOWN:
                    if (touches < 8) {
                        PWX_LOGI("input", "finger down at %.1f,%.1f",
                                 event.tfinger.x * width, event.tfinger.y * height);
                        touches++;
                    }
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    SDL_GetWindowSizeInPixels(window, &width, &height);
                    PWX_LOGI("SDL", "window resized: %dx%d", width, height);
                    break;
                case SDL_EVENT_WILL_ENTER_BACKGROUND:
                    PWX_LOGI("SDL", "entering background");
                    break;
                case SDL_EVENT_DID_ENTER_FOREGROUND:
                    PWX_LOGI("SDL", "entered foreground");
                    break;
                default:
                    break;
            }
        }

        pwx::GlRender(static_cast<float>(SDL_GetTicks() - start) / 1000.0f, width, height);
        SDL_GL_SwapWindow(window);
        frames++;

        Uint64 now = SDL_GetTicks();
        if (now - lastLog >= 2000) {
            double seconds = static_cast<double>(now - start) / 1000.0;
            PWX_LOGI("frame", "frames=%d avg_fps=%.1f", frames, frames / seconds);
            lastLog = now;
        }
    }

    // Release GL objects while the context is still current.
    pwx::GlShutdown();

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    PWX_LOGI("SDL", "clean shutdown after %d frames", frames);
    return 0;
}
