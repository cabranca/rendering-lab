# ----------------------------------------------------------------------------
# Third-party libraries bundled with the lab (not part of the Vulkan SDK)
# ----------------------------------------------------------------------------

# KTX: GPU texture container loading
set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/ktx)
add_library(ktx STATIC
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c
    ${KTX_DIR}/lib/vkloader.c)
target_include_directories(ktx SYSTEM PUBLIC
    ${KTX_DIR}/include
    ${KTX_DIR}/other_include)
if (APPLE)
    target_link_libraries(ktx PUBLIC $ENV{VULKAN_SDK}/lib/libvulkan.dylib)
elseif (NOT WIN32)
    target_link_libraries(ktx PUBLIC $ENV{VULKAN_SDK}/lib/libvulkan.so)
endif()

# ----------------------------------------------------------------------------
# rendering_core: shared Vulkan boilerplate every demo links against.
# Add common helpers (context, swapchain, pipeline, model/texture loaders, ...)
# to the source list below as the lab grows.
# ----------------------------------------------------------------------------
add_library(rendering_core STATIC
    external/tinyobj/tiny_obj_loader.cc
    src/VulkanCore.cpp
    src/VulkanContext.cpp
    src/Swapchain.cpp
)

target_include_directories(rendering_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/external/tinyobj)

target_link_libraries(rendering_core
    PUBLIC
        rendering_options
        Vulkan::Vulkan
        ktx
        ${SDL_LIBRARY}
        ${Slang_LIBRARY})

if (NOT WIN32)
    target_link_libraries(rendering_core PUBLIC glm::glm-header-only)
endif()
