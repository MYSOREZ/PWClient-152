cmake_minimum_required(VERSION 3.22)
project(pwx CXX)

add_library(pwx SHARED pwx_jni.cpp)

target_compile_features(pwx PRIVATE cxx_std_20)

find_library(log-lib log)
target_link_libraries(pwx ${log-lib} android)
