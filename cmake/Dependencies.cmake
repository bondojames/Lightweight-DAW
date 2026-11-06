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
