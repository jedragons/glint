execute_process(COMMAND ${EMULATOR} "${RUNNER}" x RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT output MATCHES "Passed: 3/7 tests"
    OR NOT error MATCHES "intentional failure"
    OR NOT error MATCHES "GUNIT_DEFER_SIZE exceeded"
    OR NOT error MATCHES "intentional cleanup failure"
    OR NOT error MATCHES "intentional helper failure")
    message(FATAL_ERROR "Unexpected unit runner result ${result}:\n${output}\n${error}")
endif()

foreach(case failure return fallthrough overflow cleanup-failure helper)
    if(NOT output MATCHES "verified ${case} cleanup")
        message(FATAL_ERROR "Missing ${case} cleanup verification:\n${output}\n${error}")
    endif()
endforeach()

execute_process(COMMAND ${EMULATOR} "${FATAL_RUNNER}" RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 1 OR NOT error MATCHES "intentional fatal failure" OR NOT error MATCHES "fatal cleanup: 9")
    message(FATAL_ERROR "Unexpected fatal cleanup result ${result}:\n${output}\n${error}")
endif()
