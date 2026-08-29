cmake_minimum_required(VERSION 3.20)
project(VektoryumV2 VERSION 0.1.0 LANGUAGES CXX)

option(VEKTORYUM_ENABLE_SANITIZERS "Enable address/undefined behavior sanitizers where supported" OFF)
option(VEKTORYUM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(vektoryum_core STATIC
    src/analysis/content_analyzer.cpp
    src/core/color.cpp
    src/core/image.cpp
    src/core/tile.cpp
    src/io/limits.cpp
    src/resample/resampler.cpp
    src/vector/path_quality.cpp
    src/vector/reconstruction.cpp
    src/version.cpp
)

target_include_directories(vektoryum_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(vektoryum_core PUBLIC cxx_std_20)

function(vektoryum_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(VEKTORYUM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wformat=2
        )
        if(VEKTORYUM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

vektoryum_apply_warnings(vektoryum_core)

if(VEKTORYUM_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(vektoryum_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(vektoryum_core PRIVATE -fsanitize=address,undefined)
endif()

add_executable(vektoryum_cli apps/vektoryum_cli/main.cpp)
target_link_libraries(vektoryum_cli PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_cli)

if(VEKTORYUM_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(vektoryum_cli PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(vektoryum_cli PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()

add_executable(vektoryum_tests
    tests/test_main.cpp
    tests/test_resampler.cpp
    tests/test_content_analyzer.cpp
    tests/test_vector_reconstruction.cpp
)
target_link_libraries(vektoryum_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_tests)

if(VEKTORYUM_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(vektoryum_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(vektoryum_tests PRIVATE -fsanitize=address,undefined)
endif()

add_test(NAME vektoryum.unit COMMAND vektoryum_tests)
