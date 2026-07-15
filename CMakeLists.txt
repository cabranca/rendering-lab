cmake_minimum_required(VERSION 3.21)
project(RenderingLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

find_library(GLSL_LIBRARY                  NAMES glslang                                 HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)
find_library(GLSL_RESOURCE_LIMITS_LIBRARY  NAMES glslang-default-resource-limits         HINTS "$ENV{VULKAN_SDK}/lib" REQUIRED)

find_program(SLANGC_EXECUTABLE             NAMES slangc                                  HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)

# SDL3 ships with the Vulkan SDK as SDL3.xcframework (dynamic); its config defines
# SDL3::SDL3 as an alias for the shared framework target. Point at the config embedded
# in the macOS framework slice, not the SDK's top-level cmake/SDL3Config.cmake: that one
# is mispackaged (its version file looks for the xcframework one dir up and fails), while
# the embedded copy sits next to its framework and resolves correctly.
set(SDL3_DIR "$ENV{VULKAN_SDK}/lib/SDL3.xcframework/macos-arm64_x86_64/SDL3.framework/Resources/CMake")
find_package(SDL3 REQUIRED)

# Shader compilation
function (add_slang_shader_target TARGET)
  cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/assets/shaders)
  set (ENTRY_POINTS -entry vertMain -entry fragMain)
  set (SPV_FILES)
  foreach (SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component (SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    set (SPV_FILE ${SHADERS_DIR}/${SHADER_NAME}.spv)
    add_custom_command (
            OUTPUT  ${SPV_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCE} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SPV_FILE}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling Slang shader ${SHADER_NAME}.slang"
            VERBATIM
    )
    list (APPEND SPV_FILES ${SPV_FILE})
  endforeach()
  add_custom_target (${TARGET} DEPENDS ${SPV_FILES})
endfunction()

# CONFIGURE_DEPENDS re-runs the glob on build, so new .slang files are picked up
# without a manual re-configure
file(GLOB SLANG_SHADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/assets/shaders/*.slang)
add_slang_shader_target(shaders SOURCES ${SLANG_SHADERS})

# SOURCE_DIR keeps the spdlog checkout inside the repo so its headers are browsable
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG v1.15.3
    GIT_SHALLOW ON
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/spdlog
)
FetchContent_MakeAvailable(spdlog)

add_executable(rendering-lab src/Main.cpp
    src/Window.cpp
    src/Metal/MetalEngine.cpp
    src/Logger.cpp
    src/Metal/MetalTexture.cpp
    src/stb_image.cpp
    src/Vulkan/VulkanEngine.cpp
    src/Vulkan/VulkanTexture.cpp
    src/Utils.cpp)

# Compile the Slang shaders to SPIR-V before building the executable, so the
# asset copy below picks up the generated .spv files
add_dependencies(rendering-lab shaders)

# src/ on the include path lets headers in subdirectories (e.g. Math/) include
# project-root headers like "Assertion.h" directly
target_include_directories(rendering-lab PRIVATE ${CMAKE_SOURCE_DIR}/src)

# metal-cpp is header-only; SYSTEM silences warnings from its headers
target_include_directories(rendering-lab SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/external/metal-cpp)

target_link_libraries(rendering-lab PRIVATE
    Vulkan::Vulkan
    ${GLSL_LIBRARY}
    ${GLSL_RESOURCE_LIMITS_LIBRARY}
    SDL3::SDL3
    spdlog::spdlog
    "-framework Foundation"
    "-framework Metal"
    "-framework QuartzCore"
)

# Copy assets next to the binary so relative paths work when running from the build dir.
# This is a target rather than a POST_BUILD command because POST_BUILD only runs when the
# executable relinks: editing an asset alone leaves the sources untouched, so the build
# would report "no work to do" and never copy. A target with no OUTPUT is always stale,
# and copy_directory_if_different keeps the copy itself cheap.
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:rendering-lab>/assets
    COMMENT "Copying assets to build directory"
)
# Shaders must be compiled before the copy, and the copy must precede a run of the binary
add_dependencies(copy_assets shaders)
add_dependencies(rendering-lab copy_assets)
