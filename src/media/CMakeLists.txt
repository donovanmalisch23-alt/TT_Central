# ttc_media — TT Central's media bot (GPL-3.0-or-later): a native C++ port of
# Simple TT Media. ttc-mediabot runs as a separate process that TT Central
# starts and controls; it decodes with ffmpeg and plays into the channel as
# voice through TT_InsertAudioBlock.

add_library(ttc_media STATIC
    ttc/media/track.h          ttc/media/track.cpp
    ttc/media/pipeprocess.h    ttc/media/pipeprocess.cpp
    ttc/media/audioengine.h    ttc/media/audioengine.cpp
    ttc/media/player.h         ttc/media/player.cpp
    ttc/media/ytdlp.h          ttc/media/ytdlp.cpp
    ttc/media/sources.h        ttc/media/sources.cpp
    ttc/media/commands.h       ttc/media/commands.cpp
    ttc/media/botconfig.h      ttc/media/botconfig.cpp
    ttc/media/mediabot.h       ttc/media/mediabot.cpp
    ttc/media/controlserver.h  ttc/media/controlserver.cpp
)
set_target_properties(ttc_media PROPERTIES AUTOMOC ON)
target_include_directories(ttc_media PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ttc_media PUBLIC ttc_core Qt6::Core Qt6::Network)
target_compile_definitions(ttc_media PRIVATE TTC_VERSION="${PROJECT_VERSION}")
find_package(Threads REQUIRED)
target_link_libraries(ttc_media PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(ttc_media PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ttc_media PRIVATE -Wall -Wextra)
endif()

add_executable(ttc-mediabot app/main.cpp)
target_link_libraries(ttc-mediabot PRIVATE ttc_media)
if(MSVC)
    target_compile_options(ttc-mediabot PRIVATE /utf-8)
endif()
if(WIN32)
    # The SDK runtime must sit next to the executable.
    add_custom_command(TARGET ttc-mediabot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_PROPERTY:teamtalk::sdk,IMPORTED_LOCATION>" "$<TARGET_FILE_DIR:ttc-mediabot>"
        VERBATIM)
endif()
if(TTC_BUILD_CLIENT)
    # TT Central starts the bot from the client's own folder.
    add_custom_command(TARGET ttc-mediabot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ttc-mediabot>" "${TTC_CLIENT_DIR}/"
        VERBATIM)
endif()
