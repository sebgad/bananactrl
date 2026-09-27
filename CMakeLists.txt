cmake_minimum_required(VERSION 3.22)

# Local, git-ignored overrides (Wi-Fi factory credentials, Pt1000 coefficients, ...)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.local")
    list(APPEND SDKCONFIG_DEFAULTS "sdkconfig.defaults.local")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bananactrl)

# Web UI in data/ -> LittleFS image, flashed together with the app
littlefs_create_partition_image(storage data FLASH_IN_PROJECT)
