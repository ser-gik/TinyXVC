
execute_process(COMMAND git config core.hooksPath ${CMAKE_CURRENT_LIST_DIR}/../git-hooks)

function(get_txvc_version_from_git project_version_var project_version_short_var)
    execute_process(COMMAND git describe --tags --dirty
        OUTPUT_VARIABLE description
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (description MATCHES "v(([0-9]+\.[0-9]+\.[0-9]+).*)")
        set(${project_version_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
        set(${project_version_short_var} ${CMAKE_MATCH_2} PARENT_SCOPE)
        return()
    endif()
    message(FATAL_ERROR "Bad Git revision format:\"${description}\"")
endfunction()

