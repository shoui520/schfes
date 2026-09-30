# Run only in a fetched dependency tree, never in the project's source tree.
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --check "${PATCH_FILE}"
 WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE can_apply ERROR_VARIABLE apply_error)
if(can_apply EQUAL 0)
 execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${PATCH_FILE}"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE applied ERROR_VARIABLE apply_error)
 if(NOT applied EQUAL 0)
  message(FATAL_ERROR "Failed to apply ${PATCH_FILE}: ${apply_error}")
 endif()
else()
 execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
  WORKING_DIRECTORY "${SOURCE_DIR}" RESULT_VARIABLE already_applied OUTPUT_QUIET ERROR_QUIET)
 if(NOT already_applied EQUAL 0)
  message(FATAL_ERROR "Dependency patch does not match ${SOURCE_DIR}. Use a fresh build directory after changing dependency pins or patches. ${apply_error}")
 endif()
endif()
