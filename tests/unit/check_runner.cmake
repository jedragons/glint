execute_process(COMMAND ${EMULATOR} "${RUNNER}" x
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result STREQUAL "1" OR NOT output MATCHES "[\r\n]    Passed: 3/7 tests[.]$"
    OR NOT error MATCHES "intentional failure"
    OR NOT error MATCHES "GUNIT_DEFER_SIZE exceeded"
    OR NOT error MATCHES "intentional cleanup failure"
    OR NOT error MATCHES "intentional helper failure"
    OR error MATCHES "early return stops the body|overflow stops the body|caller failure stops the test")
    message(FATAL_ERROR "Unexpected unit runner result ${result}:\n${output}\n${error}")
endif()

foreach(case failure return fallthrough overflow cleanup-failure helper)
    if(NOT output MATCHES "verified ${case} cleanup")
        message(FATAL_ERROR "Missing ${case} cleanup verification:\n${output}\n${error}")
    endif()
endforeach()

execute_process(COMMAND ${EMULATOR} "${FATAL_RUNNER}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error OUTPUT_STRIP_TRAILING_WHITESPACE)
# Returning to the runner would print the case result and test summary.
if(NOT result STREQUAL "1" OR NOT output STREQUAL "    Running 'fatal_failure_runs_cleanup'..."
    OR NOT error MATCHES "intentional fatal failure" OR NOT error MATCHES "fatal cleanup: 9")
    message(FATAL_ERROR "Unexpected fatal cleanup result ${result}:\n${output}\n${error}")
endif()
