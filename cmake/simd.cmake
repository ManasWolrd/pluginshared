function(simd_collect_file output_files_var dispatch_defines_var vec4_cpp vec4_2_cpp vec8_cpp vec8_2_cpp)
    set(tmp_files "")
    set(tmp_defines "")

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        list(APPEND tmp_files ${vec4_cpp})
        list(APPEND tmp_defines "VEC4_DISPATCH_INSTRUCTIONS=NEON")
    else()
        list(APPEND tmp_files ${vec4_cpp})
        list(APPEND tmp_defines "VEC4_DISPATCH_INSTRUCTIONS=SSE2")
        set_source_files_properties(${vec4_cpp} PROPERTIES COMPILE_FLAGS "-msse2")

        list(APPEND tmp_files ${vec4_2_cpp})
        list(APPEND tmp_defines "VEC4_2_DISPATCH_INSTRUCTIONS=SSE4_1")
        set_source_files_properties(${vec4_2_cpp} PROPERTIES COMPILE_FLAGS "-msse4.1")

        list(APPEND tmp_files ${vec8_cpp})
        list(APPEND tmp_defines "VEC8_DISPATCH_INSTRUCTIONS=AVX")
        set_source_files_properties(${vec8_cpp} PROPERTIES COMPILE_FLAGS "-mavx")

        list(APPEND tmp_files ${vec8_2_cpp})
        list(APPEND tmp_defines "VEC8_2_DISPATCH_INSTRUCTIONS=AVX2")
        set_source_files_properties(${vec8_2_cpp} PROPERTIES COMPILE_FLAGS "-mavx2")
    endif()
    
    set(${output_files_var} ${tmp_files} PARENT_SCOPE)
    set(${dispatch_defines_var} ${tmp_defines} PARENT_SCOPE)
endfunction()
