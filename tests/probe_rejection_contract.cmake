if(NOT DEFINED CLI_PATH)
    if(NOT DEFINED BUILD_DIR)
        message(FATAL_ERROR "BUILD_DIR or CLI_PATH is required")
    endif()
    if(WIN32)
        set(CLI "${BUILD_DIR}/Release/vektoryum_cli.exe")
    else()
        set(CLI "${BUILD_DIR}/vektoryum_cli")
    endif()
else()
    set(CLI "${CLI_PATH}")
endif()

if(NOT EXISTS "${CLI}")
    message(FATAL_ERROR "CLI executable not found: ${CLI}")
endif()

find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
set(fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/probe-rejection-contract")
file(REMOVE_RECURSE "${fixture_dir}")
file(MAKE_DIRECTORY "${fixture_dir}")

function(write_base64_fixture name payload)
    set(path "${fixture_dir}/${name}")
    execute_process(
        COMMAND "${PYTHON_EXECUTABLE}" -c "import base64,sys;open(sys.argv[1],'wb').write(base64.b64decode(sys.argv[2]))" "${path}" "${payload}"
        RESULT_VARIABLE fixture_rc
        ERROR_VARIABLE fixture_err)
    if(NOT fixture_rc EQUAL 0 OR NOT EXISTS "${path}")
        message(FATAL_ERROR "fixture generation failed for ${name}: rc=${fixture_rc} stderr=[${fixture_err}]")
    endif()
endfunction()

# These are structurally recognized real-world variants intentionally outside the
# decoder's supported baseline subset. Probe must therefore report an error,
# never status=accepted, because --convert cannot decode them.
write_base64_fixture("palette.png" "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAMAAAAoyzS7AAAAA1BMVEX/AAAZ4gk3AAAACklEQVR4nGNgAAAAAgABSK+kcQAAAABJRU5ErkJggg==")
write_base64_fixture("progressive.jpg" "/9j/2wBDAAEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/wgALCAABAAEBAREA/8QAFAABAAAAAAAAAAAAAAAAAAAAAP/EABQQAQAAAAAAAAAAAAAAAAAAAAD/2gAIAQEAAD8AP//Z")
write_base64_fixture("lossy.webp" "UklGRhQAAABXRUJQVlA4IAgAAAAAAACdASoBAA==")
write_base64_fixture("lzw.tiff" "SUkqAAgAAAAKAAABBAABAAAAAgAAAAEBBAABAAAAAQAAAAIBAwAEAAAAhgAAAAMBAwABAAAABQAAAAYBAwABAAAAAgAAABEBBAABAAAAjgAAABUBAwABAAAABAAAABcBBAABAAAACAAAABwBAwABAAAAAQAAAFIBAwABAAAAAgAAAAAAAAAIAAgACAAIAEAgEIAKFB7/")

foreach(name IN ITEMS palette.png progressive.jpg lossy.webp lzw.tiff)
    execute_process(
        COMMAND "${CLI}" --probe-input "${fixture_dir}/${name}"
        RESULT_VARIABLE probe_rc
        OUTPUT_VARIABLE probe_out
        ERROR_VARIABLE probe_err)
    if(probe_rc EQUAL 0)
        message(FATAL_ERROR "${name}: --probe-input must reject a recognized but undecodable variant; stdout=[${probe_out}]")
    endif()
    if(probe_out MATCHES "status=accepted")
        message(FATAL_ERROR "${name}: --probe-input must never claim accepted for an undecodable variant; stdout=[${probe_out}]")
    endif()
    if(NOT probe_out MATCHES "^schema_version=vektoryum\\.raster-input\\.v1\nstatus=error\nerror=(unsupported_feature|malformed_container|truncated_pixel_data)\n$")
        message(FATAL_ERROR "${name}: probe rejection must use the canonical raster-input error contract; rc=${probe_rc} stdout=[${probe_out}] stderr=[${probe_err}]")
    endif()
    if(NOT probe_err STREQUAL "")
        message(FATAL_ERROR "${name}: probe rejection must remain stdout-only; stderr=[${probe_err}]")
    endif()
endforeach()

file(REMOVE_RECURSE "${fixture_dir}")
message(STATUS "probe rejection regression fixtures passed")
