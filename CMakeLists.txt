# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.28)
project(LightweightDAW VERSION 0.0.0 LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "LightweightDAW targets Windows x64 with MSVC only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "LightweightDAW is x64 only. Run from an x64 MSVC environment (tools/build.cmd does this).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT: the portable folder needs no VC++ redistributable (ADR-006).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

enable_testing()

find_package(Git REQUIRED)
execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=12 HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE LDAW_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT LDAW_COMMIT)
  set(LDAW_COMMIT "uncommitted")
endif()
configure_file(cmake/BuildInfo.h.in ${CMAKE_BINARY_DIR}/generated/BuildInfo.h)

add_library(ldaw_warnings INTERFACE)
target_compile_options(ldaw_warnings INTERFACE /W4 /WX /wd4324 /permissive- /utf-8 /Zc:__cplusplus)
target_compile_definitions(ldaw_warnings INTERFACE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(ldaw_warnings INTERFACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/generated)

# --- Layering (spec section 3) is enforced by what each target links. ---
# core / engine / plugins: no ImGui, no D3D11, no window code.
add_library(ldaw_core STATIC
  core/settings/KeyValueStore.cpp
  core/layout/Geometry.cpp
  core/layout/LayoutFile.cpp
  core/logging/Log.cpp
)
target_link_libraries(ldaw_core PUBLIC ldaw_warnings)

add_library(ldaw_engine STATIC
  engine/AudioEngine.cpp
  engine/render/OfflineRender.cpp
  engine/device/WasapiBackend.cpp
  engine/device/AudioService.cpp
)
target_link_libraries(ldaw_engine PUBLIC ldaw_warnings PRIVATE avrt ole32 uuid)

# Real-device test tool. Links no ImGui or Direct3D.
add_executable(LightweightDAW_audiotest tools/audiotest/main.cpp)
target_link_libraries(LightweightDAW_audiotest PRIVATE ldaw_engine ole32 uuid)

add_library(ldaw_engine_api INTERFACE)
target_link_libraries(ldaw_engine_api INTERFACE ldaw_warnings)
add_library(ldaw_plugins_api INTERFACE)
target_link_libraries(ldaw_plugins_api INTERFACE ldaw_warnings)

# Scheduling policy is pure logic so it can be tested without a window.
add_library(ldaw_ui_logic STATIC ui/FrameScheduler.cpp)
target_link_libraries(ldaw_ui_logic PUBLIC ldaw_warnings)

add_subdirectory(tests)

option(LDAW_BUILD_APP "Build the GUI application" ON)
if(LDAW_BUILD_APP)
  include(cmake/Dependencies.cmake)

  add_library(ldaw_platform STATIC
    platform/win32/WindowHost.cpp
    platform/win32/NativeDialogs.cpp
    platform/win32/ShellIntegration.cpp
    platform/win32/ProcessMetrics.cpp
    platform/win32/CrashHandler.cpp
  )
  target_link_libraries(ldaw_platform PUBLIC ldaw_core PRIVATE dbghelp psapi shell32 ole32 uuid)

  add_library(ldaw_renderer STATIC
    renderer/d3d11/D3D11Device.cpp
    renderer/d3d11/SwapChainManager.cpp
  )
  target_link_libraries(ldaw_renderer PUBLIC ldaw_warnings d3d11 dxgi)

  add_executable(LightweightDAW WIN32
    app/WinMain.cpp
    app/Application.cpp
    app/CommandLine.cpp
    app/UiSelfTest.cpp
    ui/ImGuiHost.cpp
    ui/WorkspaceManager.cpp
    ui/Theme.cpp
    ui/panels/Panels.cpp
    app/LightweightDAW.manifest
  )
  target_link_libraries(LightweightDAW PRIVATE
    ldaw_core ldaw_engine ldaw_engine_api ldaw_plugins_api ldaw_ui_logic ldaw_platform ldaw_renderer imgui_docking)

  # Launches the real window on the software rasterizer, so this also runs on CI
  # runners without a GPU. Hardware runs are measured by tools/profile.ps1.
  add_test(NAME app_smoke_warp
           COMMAND LightweightDAW --warp --smoke-seconds 3 --data-dir ${CMAKE_BINARY_DIR}/smoke-data)
  set_tests_properties(app_smoke_warp PROPERTIES TIMEOUT 60)

  # Scripted checks of docking, workspaces and recovery, on real ImGui state.
  add_test(NAME app_ui_self_test
           COMMAND ${CMAKE_COMMAND} -DAPP=$<TARGET_FILE:LightweightDAW> -DDATA=${CMAKE_BINARY_DIR}/selftest-data -P ${CMAKE_SOURCE_DIR}/tests/ui/RunSelfTest.cmake)
  set_tests_properties(app_ui_self_test PROPERTIES TIMEOUT 120)
endif()
