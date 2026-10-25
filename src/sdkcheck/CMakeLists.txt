# ttc-sdkcheck — minimal SDK smoke test (no Qt dependency).
# Builds a tiny console program that prints the loaded TeamTalk SDK version.

add_executable(ttc-sdkcheck main.cpp)
target_link_libraries(ttc-sdkcheck PRIVATE teamtalk::sdk)

# Put the TeamTalk runtime DLL next to the executable so it can run.
if(WIN32)
    add_custom_command(TARGET ttc-sdkcheck POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PROPERTY:teamtalk::sdk,IMPORTED_LOCATION>"
            "$<TARGET_FILE_DIR:ttc-sdkcheck>"
        VERBATIM)
endif()
