#include "pwx_log.h"

#include <android/log.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

namespace pwx {
namespace {

std::mutex g_mutex;
FILE* g_file = nullptr;
std::string g_path;

void Timestamp(char* out, size_t size) {
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    tm local{};
    localtime_r(&ts.tv_sec, &local);
    snprintf(out, size, "%02d:%02d:%02d.%03d",
             local.tm_hour, local.tm_min, local.tm_sec,
             static_cast<int>(ts.tv_nsec / 1000000));
}

}  // namespace

void SetLogFile(const char* path) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
    g_path = path ? path : "";
    if (!g_path.empty()) {
        g_file = fopen(g_path.c_str(), "a");
    }
    __android_log_print(ANDROID_LOG_INFO, "PWX-native", "log file: %s (%s)",
                        g_path.c_str(), g_file ? "ok" : "FAILED");
    if (g_file) {
        fprintf(g_file, "[----] I/PwxLog: native log attached\n");
        fflush(g_file);
    }
}

void Log(const char* level, const char* tag, const char* fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    int prio = ANDROID_LOG_INFO;
    if (level[0] == 'W') {
        prio = ANDROID_LOG_WARN;
    } else if (level[0] == 'E') {
        prio = ANDROID_LOG_ERROR;
    }
    __android_log_print(prio, "PWX-native", "%s: %s", tag, message);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_file) {
        return;
    }
    char stamp[16];
    Timestamp(stamp, sizeof(stamp));
    fprintf(g_file, "[%s] %s/%s: %s\n", stamp, level, tag, message);
    fflush(g_file);  // a crash must not lose the last lines
}

}  // namespace pwx
