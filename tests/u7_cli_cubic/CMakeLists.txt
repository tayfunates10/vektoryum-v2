cmake_minimum_required(VERSION 3.20)
project(VektoryumU7CliCubicRegression LANGUAGES CXX)

add_executable(vektoryum_u7_cli_cubic_test ../test_cli_cubic_production.cpp)
target_include_directories(vektoryum_u7_cli_cubic_test PRIVATE ..)
target_compile_features(vektoryum_u7_cli_cubic_test PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(vektoryum_u7_cli_cubic_test PRIVATE /W4 /permissive- /WX)
else()
    target_compile_options(vektoryum_u7_cli_cubic_test PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Werror)
endif()
