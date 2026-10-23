#pragma once

namespace pwx {

// File sink for the on-device log. Path comes from the Java side (PwxLog) so native and
// Java lines land in the same file. Safe to call before any logging.
void SetLogFile(const char* path);

// Writes one line to logcat and, when a path was set, to the log file.
void Log(const char* level, const char* tag, const char* fmt, ...)
        __attribute__((format(printf, 3, 4)));

}  // namespace pwx

#define PWX_LOGI(tag, ...) ::pwx::Log("I", tag, __VA_ARGS__)
#define PWX_LOGW(tag, ...) ::pwx::Log("W", tag, __VA_ARGS__)
#define PWX_LOGE(tag, ...) ::pwx::Log("E", tag, __VA_ARGS__)
