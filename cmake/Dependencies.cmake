# SPDX-License-Identifier: GPL-3.0-or-later
# Dependencies are pinned to exact commits (ADR-003). Do not use branch names.
include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/.deps CACHE PATH "Shared dependency checkout" FORCE)

# Dear ImGui v1.93.0-docking (MIT). Annotated tag 4e02e84c resolves to this commit.
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        b916186e66aa3c02b72c6ca17455516512ed8049
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_docking STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_docking PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_docking PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS UNICODE _UNICODE)
target_compile_options(imgui_docking PRIVATE /utf-8 /W0)
target_link_libraries(imgui_docking PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32)

# libsndfile 1.2.2 (LGPL-2.1-or-later): WAV and AIFF. Built as a DLL so users can replace
# it, as the LGPL requires. External codecs are off: FLAC, MP3 and M4A go through
# Windows Media Foundation (ADR-018).
# These projects still declare support back to CMake 3.1, which CMake 4 refuses by default.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# Its own DLL build needs Python, so build it as a library and wrap that in a DLL below.
set(ENABLE_EXTERNAL_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_MPEG OFF CACHE BOOL "" FORCE)
set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_CPACK OFF CACHE BOOL "" FORCE)
set(ENABLE_PACKAGE_CONFIG OFF CACHE BOOL "" FORCE)
set(INSTALL_PKGCONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(INSTALL_MANPAGES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sndfile
  GIT_REPOSITORY https://github.com/libsndfile/libsndfile.git
  GIT_TAG        72f6af15e8f85157bd622ed45b979025828b7001
)
FetchContent_MakeAvailable(sndfile)

file(WRITE ${CMAKE_BINARY_DIR}/generated/sndfile_dll.c "/* The code comes from the wrapped library. */
")
add_library(ldaw_sndfile_dll SHARED ${CMAKE_BINARY_DIR}/generated/sndfile_dll.c ${CMAKE_SOURCE_DIR}/cmake/sndfile.def)
set_target_properties(ldaw_sndfile_dll PROPERTIES OUTPUT_NAME sndfile ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sndfile_import)
add_dependencies(ldaw_sndfile_dll sndfile)
target_link_options(ldaw_sndfile_dll PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:sndfile>)
target_link_libraries(ldaw_sndfile_dll PRIVATE $<TARGET_FILE:sndfile>)
target_include_directories(ldaw_sndfile_dll INTERFACE ${sndfile_SOURCE_DIR}/include ${sndfile_BINARY_DIR}/include ${sndfile_BINARY_DIR}/src)

# libsamplerate 0.2.2 (BSD-2-Clause), static.
set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(samplerate
  GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
  GIT_TAG        c96f5e3de9c4488f4e6c97f59f5245f22fda22f7
)
FetchContent_MakeAvailable(samplerate)
unset(CMAKE_POLICY_VERSION_MINIMUM)
