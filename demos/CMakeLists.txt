# add_demo(<TargetName> <source files...>)
#
# Registers a demo executable that links the shared rendering_core library.
# Any files in the demo's local "assets/" directory are copied next to the
# built binary (build/bin) so it can be run directly, and the debugger working
# directory is pointed at the demo source so relative asset paths resolve.
function(add_demo NAME)
    add_executable(${NAME} ${ARGN})
    target_link_libraries(${NAME} PRIVATE rendering_core)
    set_target_properties(${NAME} PROPERTIES
        DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        add_custom_command(TARGET ${NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:${NAME}>/assets"
            COMMENT "Copying assets for ${NAME}")
    endif()
endfunction()

# One subdirectory per demo. Add new demos here.
add_subdirectory(cubemap)
