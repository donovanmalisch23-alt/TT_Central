# TT Central — top-level build
#
# Prerequisites (see docs/SDK.md and docs/PHASE1.md):
#   git submodule update --init --depth 1
#   python -m pip install py7zr
#   python scripts/bootstrap.py        # fetch SDK, stage it, apply patches
#
# Then:
#   cmake -S . -B build -A x64
#   cmake --build build --config Release

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

# --- TeamTalk 5 SDK (fetched, not committed) --------------------------------
# Proprietary (c) BearWare.dk. Run `python scripts/bootstrap.py` first; the SDK
# is git-ignored and never stored in this public repo. See docs/SDK.md.
set(TEAMTALK_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/teamtalk-sdk")
if(NOT EXISTS "${TEAMTALK_SDK_DIR}/include/TeamTalk.h")
    message(FATAL_ERROR
        "TeamTalk SDK not found under third_party/teamtalk-sdk/. "
        "Run: python scripts/bootstrap.py")
endif()
add_library(teamtalk::sdk SHARED IMPORTED)
set_target_properties(teamtalk::sdk PROPERTIES
    IMPORTED_LOCATION   "${TEAMTALK_SDK_DIR}/bin/TeamTalk5.dll"
    IMPORTED_IMPLIB     "${TEAMTALK_SDK_DIR}/lib/TeamTalk5.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${TEAMTALK_SDK_DIR}/include")

# --- SDK smoke test (milestone 1.0; no Qt needed) ---------------------------
add_subdirectory(src/sdkcheck)

# --- TT Central client: BearWare's qtTeamTalk + our patches (milestone 1.1) --
# Upstream source is the pinned submodule external/TeamTalk5 (GPLv3 client).
# scripts/bootstrap.py stages the Standard SDK (TeamTalk5.dll/.lib) into it and
# applies patches/TeamTalk5/ (if any).
if(WIN32)
    set(_ttc_client_default ON)
else()
    set(_ttc_client_default OFF)
endif()
option(TTC_BUILD_CLIENT "Build the TT Central client (qtTeamTalk-based; needs Qt 6)" ${_ttc_client_default})

if(TTC_BUILD_CLIENT)
    set(TTC_UPSTREAM_DIR "${CMAKE_SOURCE_DIR}/external/TeamTalk5")

    if(NOT EXISTS "${TTC_UPSTREAM_DIR}/Client/qtTeamTalk/CMakeLists.txt")
        message(FATAL_ERROR
            "external/TeamTalk5 is missing. Run:\n"
            "  git submodule update --init --depth 1\n"
            "  python scripts/bootstrap.py")
    endif()
    if(NOT EXISTS "${TTC_UPSTREAM_DIR}/Library/TeamTalk_DLL/TeamTalk5.lib")
        message(FATAL_ERROR
            "The SDK is not staged for the client build. Run: python scripts/bootstrap.py")
    endif()

    # Fail early and clearly if Qt is missing (qtTeamTalk itself would just
    # skip silently). TextToSpeech requires Qt 6.4+.
    find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Xml Network Multimedia TextToSpeech)

    # Prism: screen-reader output (NVDA, JAWS, SAPI and more). Accessibility is
    # a first-class requirement, so it is always built in.
    set(BUILD_TEAMTALK_CLIENT_PRISM ON)
    add_subdirectory("${TTC_UPSTREAM_DIR}/Client/Prism" "${CMAKE_BINARY_DIR}/upstream/Prism")

    add_subdirectory("${TTC_UPSTREAM_DIR}/Client/qtTeamTalk" "${CMAKE_BINARY_DIR}/upstream/qtTeamTalk")
    message(STATUS "TT Central: building the client from external/TeamTalk5 (Qt ${Qt6_VERSION})")
else()
    message(STATUS "TT Central: client build disabled (TTC_BUILD_CLIENT=OFF)")
endif()

# --- Our modules (added per phase; see docs/ROADMAP.md) ---------------------
# add_subdirectory(src/core)     # tt-core    : SDK wrapper
# add_subdirectory(src/notify)   # ttc-notify : sounds + TTS + toasts
# add_subdirectory(src/monitor)  # ttc-monitor: background watchers
# add_subdirectory(src/server)   # ttc-server : host + remote admin
# add_subdirectory(src/media)    # ttc-media  : native media engine
