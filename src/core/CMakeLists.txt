# ttc_core — TT Central's UI-free logic (GPL-3.0-or-later): SDK wrapper, monitor sessions,
# text reassembly, notifications text, and local-server configuration.

add_library(ttc_core STATIC
    ttc/sdk.h                ttc/sdk.cpp
    ttc/teamtalkclient.h     ttc/sdkteamtalkclient.cpp
    ttc/textassembler.h      ttc/textassembler.cpp
    ttc/monitorconfig.h      ttc/monitorconfig.cpp
    ttc/monitorevent.h       ttc/monitorevent.cpp
    ttc/monitorsession.h     ttc/monitorsession.cpp
    ttc/serverconfig.h       ttc/serverconfig.cpp
    ttc/localserver.h        ttc/localserver.cpp
)
set_target_properties(ttc_core PROPERTIES AUTOMOC ON)
target_include_directories(ttc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ttc_core PUBLIC Qt6::Core Qt6::Network Qt6::Xml teamtalk::sdk)
if(MSVC)
    target_compile_definitions(ttc_core PUBLIC UNICODE _UNICODE)
    target_compile_options(ttc_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ttc_core PRIVATE -Wall -Wextra)
endif()
