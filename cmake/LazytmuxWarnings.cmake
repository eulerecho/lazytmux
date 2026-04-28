function(lazytmux_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wconversion
                -Wsign-conversion
                -Wold-style-cast
                -Wdouble-promotion
                -Wnon-virtual-dtor
                -Werror
        )
    endif()
endfunction()
