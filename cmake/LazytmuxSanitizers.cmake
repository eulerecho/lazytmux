function(lazytmux_apply_sanitizers target)
    if(LAZYTMUX_ENABLE_ASAN AND LAZYTMUX_ENABLE_TSAN)
        message(FATAL_ERROR "ASan and TSan cannot be enabled in the same build")
    endif()

    set(sanitizers "")
    if(LAZYTMUX_ENABLE_ASAN)
        list(APPEND sanitizers "address")
    endif()
    if(LAZYTMUX_ENABLE_UBSAN)
        list(APPEND sanitizers "undefined")
    endif()
    if(LAZYTMUX_ENABLE_TSAN)
        list(APPEND sanitizers "thread")
    endif()

    if(sanitizers)
        if(MSVC)
            message(FATAL_ERROR "Sanitizer options are not configured for MSVC yet")
        endif()

        list(JOIN sanitizers "," sanitizer_arg)
        target_compile_options(${target}
            PRIVATE "-fsanitize=${sanitizer_arg}" -fno-omit-frame-pointer)
        target_link_options(${target}
            PRIVATE "-fsanitize=${sanitizer_arg}" -fno-omit-frame-pointer)
    endif()
endfunction()
