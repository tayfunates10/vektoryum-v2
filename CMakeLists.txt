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
    src/api/stable_api.cpp
    src/certification/quality_certificate.cpp
    src/core/color.cpp
    src/core/image.cpp
    src/core/tile.cpp
    src/export/canonical_encoder.cpp
    src/export/export_artifact_contract.cpp
    src/export/export_contract.cpp
    src/export/export_destination_contract.cpp
    src/hybrid/alpha_composite.cpp
    src/hybrid/hybrid_output_contract.cpp
    src/hybrid/hybrid_plan.cpp
    src/io/limits.cpp
    src/ml/artifact_digest.cpp
    src/ml/runtime_contract.cpp
    src/release/package_contents.cpp
    src/release/package_inventory.cpp
    src/release/release_manifest.cpp
    src/resample/resampler.cpp
    src/restoration/photo_restoration.cpp
    src/training/benchmark_result_contract.cpp
    src/training/dataset_contract.cpp
    src/training/training_run_contract.cpp
    src/vector/path_quality.cpp
    src/vector/reconstruction.cpp
    src/vector/svg_path.cpp
    src/version.cpp
)

target_include_directories(vektoryum_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(vektoryum_core PUBLIC cxx_std_20)

function(vektoryum_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(VEKTORYUM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2)
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

add_executable(vektoryum_tests tests/test_main.cpp tests/test_resampler.cpp tests/test_content_analyzer.cpp tests/test_photo_restoration.cpp tests/test_ml_runtime.cpp tests/test_dataset_contract.cpp tests/test_vector_reconstruction.cpp tests/test_svg_path.cpp)
target_link_libraries(vektoryum_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_tests)

add_executable(vektoryum_training_contract_tests tests/test_training_run_contract.cpp)
target_link_libraries(vektoryum_training_contract_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_training_contract_tests)

add_executable(vektoryum_benchmark_contract_tests tests/test_benchmark_result_contract.cpp)
target_link_libraries(vektoryum_benchmark_contract_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_benchmark_contract_tests)

add_executable(vektoryum_hybrid_plan_tests tests/test_hybrid_plan.cpp)
target_link_libraries(vektoryum_hybrid_plan_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_hybrid_plan_tests)

add_executable(vektoryum_alpha_composite_tests tests/test_alpha_composite.cpp)
target_link_libraries(vektoryum_alpha_composite_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_alpha_composite_tests)

add_executable(vektoryum_hybrid_output_contract_tests tests/test_hybrid_output_contract.cpp)
target_link_libraries(vektoryum_hybrid_output_contract_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_hybrid_output_contract_tests)

add_executable(vektoryum_hybrid_mixed_content_tests tests/test_hybrid_mixed_content.cpp)
target_link_libraries(vektoryum_hybrid_mixed_content_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_hybrid_mixed_content_tests)

add_executable(vektoryum_export_contract_tests tests/test_export_contract.cpp)
target_link_libraries(vektoryum_export_contract_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_export_contract_tests)

add_executable(vektoryum_export_artifact_contract_tests tests/test_export_artifact_contract.cpp)
target_link_libraries(vektoryum_export_artifact_contract_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_export_artifact_contract_tests)

add_executable(vektoryum_canonical_encoder_tests tests/test_canonical_encoder.cpp)
target_link_libraries(vektoryum_canonical_encoder_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_canonical_encoder_tests)

add_executable(vektoryum_export_destination_contract_tests tests/test_export_destination_contract.cpp)
target_link_libraries(vektoryum_export_destination_contract_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_export_destination_contract_tests)

add_executable(vektoryum_quality_certificate_tests tests/test_quality_certificate.cpp)
target_link_libraries(vektoryum_quality_certificate_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_quality_certificate_tests)

add_executable(vektoryum_quality_certificate_e2e_tests tests/test_quality_certificate_e2e.cpp)
target_link_libraries(vektoryum_quality_certificate_e2e_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_quality_certificate_e2e_tests)

add_executable(vektoryum_stable_api_tests tests/test_stable_api.cpp)
target_link_libraries(vektoryum_stable_api_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_stable_api_tests)

add_executable(vektoryum_release_manifest_tests tests/test_release_manifest.cpp)
target_link_libraries(vektoryum_release_manifest_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_release_manifest_tests)

add_executable(vektoryum_package_inventory_tests tests/test_package_inventory.cpp)
target_link_libraries(vektoryum_package_inventory_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_package_inventory_tests)

add_executable(vektoryum_package_contents_tests tests/test_package_contents.cpp)
target_link_libraries(vektoryum_package_contents_tests PRIVATE vektoryum_core)
vektoryum_apply_warnings(vektoryum_package_contents_tests)

if(VEKTORYUM_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target
        vektoryum_tests
        vektoryum_training_contract_tests
        vektoryum_benchmark_contract_tests
        vektoryum_hybrid_plan_tests
        vektoryum_alpha_composite_tests
        vektoryum_hybrid_output_contract_tests
        vektoryum_hybrid_mixed_content_tests
        vektoryum_export_contract_tests
        vektoryum_export_artifact_contract_tests
        vektoryum_canonical_encoder_tests
        vektoryum_export_destination_contract_tests
        vektoryum_quality_certificate_tests
        vektoryum_quality_certificate_e2e_tests
        vektoryum_stable_api_tests
        vektoryum_release_manifest_tests
        vektoryum_package_inventory_tests
        vektoryum_package_contents_tests)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

add_test(NAME vektoryum.unit COMMAND vektoryum_tests)
add_test(NAME vektoryum.training_contract COMMAND vektoryum_training_contract_tests)
add_test(NAME vektoryum.benchmark_contract COMMAND vektoryum_benchmark_contract_tests)
add_test(NAME vektoryum.hybrid_plan COMMAND vektoryum_hybrid_plan_tests)
add_test(NAME vektoryum.alpha_composite COMMAND vektoryum_alpha_composite_tests)
add_test(NAME vektoryum.hybrid_output_contract COMMAND vektoryum_hybrid_output_contract_tests)
add_test(NAME vektoryum.hybrid_mixed_content COMMAND vektoryum_hybrid_mixed_content_tests)
add_test(NAME vektoryum.export_contract COMMAND vektoryum_export_contract_tests)
add_test(NAME vektoryum.export_artifact_contract COMMAND vektoryum_export_artifact_contract_tests)
add_test(NAME vektoryum.canonical_encoder COMMAND vektoryum_canonical_encoder_tests)
add_test(NAME vektoryum.export_destination_contract COMMAND vektoryum_export_destination_contract_tests)
add_test(NAME vektoryum.quality_certificate COMMAND vektoryum_quality_certificate_tests)
add_test(NAME vektoryum.quality_certificate_e2e COMMAND vektoryum_quality_certificate_e2e_tests)
add_test(NAME vektoryum.stable_api COMMAND vektoryum_stable_api_tests)
add_test(NAME vektoryum.release_manifest COMMAND vektoryum_release_manifest_tests)
add_test(NAME vektoryum.package_inventory COMMAND vektoryum_package_inventory_tests)
add_test(NAME vektoryum.package_contents COMMAND vektoryum_package_contents_tests)
