cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(rendering-lab LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# ----------------------------------------------------------------------------
# Vulkan SDK
# SDL3, Volk, VMA, glm and Slang are shipped with the LunarG Vulkan SDK.
# ----------------------------------------------------------------------------
if (NOT IS_DIRECTORY $ENV{VULKAN_SDK})
    message(WARNING "Vulkan SDK not detected, if installed please set the VULKAN_SDK env var or manually provide the libraries")
else()
    message(STATUS "Found Vulkan SDK in $ENV{VULKAN_SDK}")
endif()

include_directories(SYSTEM $ENV{VULKAN_SDK}/include)

find_package(Vulkan REQUIRED)

# glm is header-only; on Windows it ships with the SDK, elsewhere we fetch it.
if (NOT WIN32)
    FetchContent_Declare(GLM
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG 1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
    FetchContent_MakeAvailable(GLM)
endif()

# SDL3 (windowing/input) and Slang (shader compiler) come from the SDK.
find_library(SDL_LIBRARY   NAMES SDL3                  HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
find_library(Slang_LIBRARY NAMES slang slang-compiler  HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)

# ----------------------------------------------------------------------------
# Common compile settings shared by core and every demo
# ----------------------------------------------------------------------------
add_library(rendering_options INTERFACE)
target_compile_features(rendering_options INTERFACE cxx_std_20)
# Volk loads Vulkan entry points dynamically, so we forbid static prototypes.
target_compile_definitions(rendering_options INTERFACE VK_NO_PROTOTYPES NOMINMAX _USE_MATH_DEFINES)

if (WIN32)
    target_compile_definitions(rendering_options INTERFACE VK_USE_PLATFORM_WIN32_KHR _CRT_SECURE_NO_WARNINGS)
elseif (LINUX)
    target_compile_definitions(rendering_options INTERFACE VK_USE_PLATFORM_WAYLAND_KHR)
endif()

if (MSVC)
    target_compile_options(rendering_options INTERFACE /EHsc)
endif()

# All executables land in build/bin so they sit next to their copied assets.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(core)
add_subdirectory(demos)
