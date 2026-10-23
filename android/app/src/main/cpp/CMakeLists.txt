cmake_minimum_required(VERSION 3.22)
project(pwx C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL3 is pinned by tag. tools/sdl3 is a pre-cloned checkout (created by build-apk.ps1)
# so configure works offline; without it CMake clones the tag itself.
set(_sdl_local "${CMAKE_CURRENT_LIST_DIR}/../../../../../tools/sdl3")
if(EXISTS "${_sdl_local}/CMakeLists.txt" AND NOT DEFINED FETCHCONTENT_SOURCE_DIR_SDL)
    set(FETCHCONTENT_SOURCE_DIR_SDL "${_sdl_local}" CACHE PATH "Pinned SDL3 checkout" FORCE)
endif()

# Only the shared SDL3 runtime is packaged. Tests, examples and install rules are noise here.
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL)

# SDL's Android glue loads "main" and calls SDL_main from it.
add_library(main SHARED
    pwx_jni.cpp
    pwx_log.cpp
    pwx_gl.cpp
    pwx_main.cpp)

target_compile_definitions(main PRIVATE PWX_VERSION="${PWX_VERSION}")
target_link_libraries(main PRIVATE SDL3::SDL3 GLESv3 log android)

# Keep the JNI bridge in a namespace of its own; game code will live in engine/ later.
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
