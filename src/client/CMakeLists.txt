# ttc_client — TT Central's client features (GPL-3.0-or-later, Qt Widgets): monitor
# manager, notification center, Monitors window and Settings dialog. Linked
# into the qtTeamTalk-based client through src/client/integration/.

add_library(ttc_client STATIC
    ttc/clienthooks.h
    ttc/central.h               ttc/central.cpp
    ttc/monitormanager.h        ttc/monitormanager.cpp
    ttc/notificationcenter.h    ttc/notificationcenter.cpp
    ttc/monitoreditdialog.h     ttc/monitoreditdialog.cpp
    ttc/monitorswindow.h        ttc/monitorswindow.cpp
    ttc/settingsdialog.h        ttc/settingsdialog.cpp
    ttc/a11yaudit.h             ttc/a11yaudit.cpp
    ttc/mediabotcontroller.h    ttc/mediabotcontroller.cpp
    ttc/mediabotwindow.h        ttc/mediabotwindow.cpp
    ttc/mediabotpage.h          ttc/mediabotpage.cpp
)
set_target_properties(ttc_client PROPERTIES AUTOMOC ON)
target_include_directories(ttc_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ttc_client PUBLIC ttc_core ttc_media Qt6::Widgets)
target_compile_definitions(ttc_client PRIVATE TTC_VERSION="${PROJECT_VERSION}")
if(MSVC)
    target_compile_options(ttc_client PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ttc_client PRIVATE -Wall -Wextra)
endif()
