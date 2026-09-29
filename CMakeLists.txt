cmake_minimum_required(VERSION 3.22)

# Local, git-ignored overrides (Wi-Fi factory credentials, Pt1000 coefficients, ...)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.local")
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.local")
endif()

# Firmware version (esp_app_desc, /version.json, Home Assistant): Semantic Versioning from git tags vX.Y.Z.
#   on the tag v1.2.0           -> 1.2.0
#   3 commits after it         -> 1.2.0-3-gabc1234
#   uncommitted changes        -> ...-dirty
#   no tag yet                 -> 0.0.0-gabc1234
# BANANA_VERSION in the environment overrides it (builds without .git).
if(DEFINED ENV{BANANA_VERSION})
    set(PROJECT_VER "$ENV{BANANA_VERSION}")
else()
    execute_process(COMMAND git describe --tags --match "v[0-9]*" --dirty
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                    OUTPUT_VARIABLE PROJECT_VER OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE describe_result ERROR_QUIET)
    if(describe_result EQUAL 0)
        string(REGEX REPLACE "^v" "" PROJECT_VER "${PROJECT_VER}")
    else()
        execute_process(COMMAND git describe --always --dirty --abbrev=7
                        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                        OUTPUT_VARIABLE git_hash OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        set(PROJECT_VER "0.0.0-g${git_hash}")
    endif()
    # Re-run CMake (and so re-read the version) on commits, checkouts, tags and staged changes
    execute_process(COMMAND git rev-parse --absolute-git-dir
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
                    OUTPUT_VARIABLE git_dir OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    foreach(file HEAD index packed-refs)
        if(EXISTS "${git_dir}/${file}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${git_dir}/${file}")
        endif()
    endforeach()
endif()
message(STATUS "bananactrl version: ${PROJECT_VER}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bananactrl)

# Web UI in data/ -> LittleFS image, flashed together with the app
littlefs_create_partition_image(storage data FLASH_IN_PROJECT)

# Release assets as on GitHub (bananactrl-<version>.bin, webui-<version>.tar, MD5SUMS, ...) in build/dist/,
# for the upload on the OTA page. Runs on every build: the LittleFS image above is rebuilt every time, too.
idf_build_get_property(python PYTHON)
idf_build_get_property(idf_target IDF_TARGET)
add_custom_target(dist ALL
    COMMAND ${python} ${CMAKE_CURRENT_LIST_DIR}/tools/dist.py --build-dir ${CMAKE_BINARY_DIR}
            --data-dir ${CMAKE_CURRENT_LIST_DIR}/data --version ${PROJECT_VER} --chip ${idf_target}
    DEPENDS app bootloader partition_table_bin blank_ota_data littlefs_storage_bin
    VERBATIM)
