function(lazytmux_apply_coverage target)
    if(NOT LAZYTMUX_ENABLE_COVERAGE)
        return()
    endif()

    if(MSVC)
        message(FATAL_ERROR "Coverage instrumentation is not configured for MSVC yet")
    endif()

    if(LAZYTMUX_ENABLE_TSAN)
        message(FATAL_ERROR "Coverage and TSan must use separate builds")
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
            "llvm-cov coverage requires a Clang compiler")
    endif()

    target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping -O0 -g)

    get_target_property(target_type ${target} TYPE)
    if(NOT target_type STREQUAL "STATIC_LIBRARY")
        target_link_options(${target} PRIVATE -fprofile-instr-generate)
    endif()
endfunction()
