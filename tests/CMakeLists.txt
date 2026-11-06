# SPDX-License-Identifier: GPL-3.0-or-later
# The headless test binary links no ImGui, D3D11 or window code, which proves the
# core/engine layers build and run without a GUI (spec section 2).
add_executable(LightweightDAW_tests
  unit/main.cpp
  unit/test_settings.cpp
  unit/test_geometry.cpp
  unit/test_layout.cpp
  unit/test_scheduler.cpp
  unit/test_interfaces.cpp
  unit/test_engine.cpp
  unit/test_device_logic.cpp
  unit/test_audio_service.cpp
  unit/test_media_import.cpp
  unit/test_model.cpp
  unit/test_playback.cpp
  unit/test_timeline_view.cpp
)
target_link_libraries(LightweightDAW_tests PRIVATE ldaw_core ldaw_ui_logic ldaw_engine ldaw_engine_api ldaw_plugins_api ldaw_sndfile_dll)
add_test(NAME unit COMMAND LightweightDAW_tests)
