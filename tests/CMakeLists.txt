# TT Central tests (Qt Test). Run with: ctest --test-dir build -C Release
find_package(Qt6 REQUIRED COMPONENTS Test)

# Each test also writes its Qt Test log to build/test-logs/<name>.txt, so CI
# can show exactly which check failed even if a test hangs or crashes.
set(TTC_TEST_LOG_DIR "${CMAKE_BINARY_DIR}/test-logs")
file(MAKE_DIRECTORY "${TTC_TEST_LOG_DIR}")

function(ttc_add_test name)
    add_executable(${name} ${name}.cpp ${ARGN})
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    target_link_libraries(${name} PRIVATE ttc_core Qt6::Test)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${name} PRIVATE /utf-8)  # sources are UTF-8
    endif()
    if(WIN32)
        # The SDK runtime must sit next to the test executable.
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_PROPERTY:teamtalk::sdk,IMPORTED_LOCATION>" "$<TARGET_FILE_DIR:${name}>"
            VERBATIM)
    endif()
    add_test(NAME ${name} COMMAND ${name} -o "${TTC_TEST_LOG_DIR}/${name}.txt,txt" -o "-,txt")
endfunction()

ttc_add_test(tst_core fakeclient.h)
ttc_add_test(tst_monitorsession fakeclient.h)
ttc_add_test(tst_client fakeclient.h)
target_link_libraries(tst_client PRIVATE ttc_client)
if(NOT WIN32)
    set_tests_properties(tst_client PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Against BearWare's real server (tt5srv), fetched with the SDK.
ttc_add_test(tst_integration_monitor serverharness.h)
target_compile_definitions(tst_integration_monitor PRIVATE TTC_SERVER_EXE_PATH="${TTC_SERVER_EXE}")
set_tests_properties(tst_integration_monitor PROPERTIES TIMEOUT 180 LABELS integration)
