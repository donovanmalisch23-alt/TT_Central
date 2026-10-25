# TT Central — top-level build (skeleton)
#
# This is Phase 0 scaffolding. It establishes the project, finds Qt, and wires
# in the vendored TeamTalk SDK. Module targets under src/ are added as each
# phase lands (see docs/ROADMAP.md). It is intentionally minimal and is not yet
# expected to build a full application.

cmake_minimum_required(VERSION 3.21)

project(TTCentral
    VERSION 0.0.1
    DESCRIPTION "Unified, accessible TeamTalk 5 client, server, monitors, and media bot for Windows"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(WARNING "TT Central currently targets Windows (x64). Other platforms are not supported yet.")
endif()

# --- Qt ---------------------------------------------------------------------
# find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Network)
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTORCC ON)
# set(CMAKE_AUTOUIC ON)

# --- TeamTalk 5 SDK (fetched, not committed) --------------------------------
# Proprietary (c) BearWare.dk. Run `python scripts/fetch_sdk.py` first; the SDK
# is git-ignored and never stored in this public repo. See docs/SDK.md.
set(TEAMTALK_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/teamtalk-sdk")
if(NOT EXISTS "${TEAMTALK_SDK_DIR}/include/TeamTalk.h")
    message(WARNING
        "TeamTalk SDK not found under third_party/teamtalk-sdk/. "
        "Run: python scripts/fetch_sdk.py")
endif()
# add_library(teamtalk::sdk SHARED IMPORTED)
# set_target_properties(teamtalk::sdk PROPERTIES
#     IMPORTED_LOCATION   "${TEAMTALK_SDK_DIR}/bin/TeamTalk5Pro.dll"
#     IMPORTED_IMPLIB     "${TEAMTALK_SDK_DIR}/lib/TeamTalk5Pro.lib"
#     INTERFACE_INCLUDE_DIRECTORIES "${TEAMTALK_SDK_DIR}/include")

# --- Modules (added per phase) ----------------------------------------------
# add_subdirectory(src/core)     # tt-core    : SDK wrapper
# add_subdirectory(src/notify)   # ttc-notify : sounds + TTS + toasts
# add_subdirectory(src/monitor)  # ttc-monitor: background watchers
# add_subdirectory(src/server)   # ttc-server : host + remote admin
# add_subdirectory(src/media)    # ttc-media  : native media engine
# add_subdirectory(src/app)      # ttc-shell  : the Qt application

message(STATUS "TT Central ${PROJECT_VERSION} — Phase 0 scaffolding.")
