#include <jni.h>
#include <android/log.h>

#include <sstream>
#include <string>

namespace {

const char* kTag = "PWX-native";
const char* kAbi =
#if defined(__aarch64__)
    "arm64-v8a";
#elif defined(__arm__)
    "armeabi-v7a";
#elif defined(__x86_64__)
    "x86_64";
#elif defined(__i386__)
    "x86";
#else
    "unknown";
#endif

std::string BuildInfo() {
    std::ostringstream os;
    os << "abi=" << kAbi
       << " ptr=" << (sizeof(void*) * 8u) << "bit"
       << " api=" << __ANDROID_API__
       << " cpp=" << __cplusplus;
    return os.str();
}

}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM*, void*) {
    const std::string info = BuildInfo();
    __android_log_print(ANDROID_LOG_INFO, kTag, "JNI_OnLoad %s", info.c_str());
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pwx_client_NativeBridge_buildInfo(JNIEnv* env, jclass) {
    const std::string info = BuildInfo();
    __android_log_print(ANDROID_LOG_INFO, kTag, "buildInfo %s", info.c_str());
    return env->NewStringUTF(info.c_str());
}
