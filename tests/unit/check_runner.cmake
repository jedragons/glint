# Both fixture executables fail deliberately. Check their diagnostics and exit status.
function(run_failure_fixture executable)
    execute_process(COMMAND ${EMULATOR} "${executable}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT result STREQUAL "1")
        message(FATAL_ERROR "Expected exit code 1 from ${executable}, got ${result}:\n${output}\n${error}")
    endif()

    string(REPLACE "\r\n" "\n" output "${output}")
    set(output "${output}" PARENT_SCOPE)
    set(error "${error}" PARENT_SCOPE)
endfunction()

function(expect_output pattern)
    if(NOT output MATCHES "${pattern}")
        message(FATAL_ERROR "Missing output matching '${pattern}':\n${output}\n${error}")
    endif()
endfunction()

run_failure_fixture("${RUNNER}" x)
expect_output("\n  Passed: 4/10 tests[.]$")
expect_output("(^|\n)  Running 'failure'[.][.][.] FAIL\n    [^ \n][^\n]*intentional failure")
expect_output("GUNIT_DEFER_SIZE exceeded")
expect_output("intentional cleanup failure")
expect_output("intentional helper failure")

set(section_failure "\n    > section 'first fails'[.][.][.] FAIL")
set(section_diagnostic "\n      [^ \n][^\n]*intentional section failure")
expect_output("${section_failure}${section_diagnostic}\n    > section 'second passes'[.][.][.] OK")
expect_output("\n    > section 'one'[.][.][.] OK\n    > section 'two'[.][.][.] OK")
expect_output("Running 'discovery_failure'[.][.][.] FAIL\n    [^ \n][^\n]*intentional discovery failure")

foreach(unreachable
        "early return stops the body"
        "overflow stops the body"
        "caller failure stops the test"
        "section 'unreachable'")
    if(output MATCHES "${unreachable}")
        message(FATAL_ERROR "Unexpected execution of '${unreachable}':\n${output}\n${error}")
    endif()
endforeach()

foreach(case failure return fallthrough overflow cleanup-failure helper)
    if(NOT error MATCHES "verified ${case} cleanup")
        message(FATAL_ERROR "Missing ${case} cleanup verification:\n${output}\n${error}")
    endif()
endforeach()

run_failure_fixture("${FATAL_RUNNER}")
expect_output("^  Running 'fatal_failure_runs_cleanup'[.][.][.] FAIL")
expect_output("intentional fatal failure")
if(output MATCHES "Passed:" OR NOT error MATCHES "fatal cleanup: 9")
    message(FATAL_ERROR "Fatal failure must run cleanup and exit before the summary:\n${output}\n${error}")
endif()
