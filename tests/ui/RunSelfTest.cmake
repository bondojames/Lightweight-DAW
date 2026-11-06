# SPDX-License-Identifier: GPL-3.0-or-later
# Starts from an empty data folder and prints the app's log, so a failure names its step.
file(REMOVE_RECURSE "${DATA}")
execute_process(COMMAND "${APP}" --warp --ui-self-test --data-dir "${DATA}" RESULT_VARIABLE result)
file(READ "${DATA}/logs/app.log" log)
message(STATUS "${log}")
if(NOT result EQUAL 0)
  message(FATAL_ERROR "UI self-test exited with ${result}")
endif()
