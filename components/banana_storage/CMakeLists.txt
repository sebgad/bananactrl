idf_component_register(SRCS "src/File.cpp"
                            "src/FileLogger.cpp"
                            "src/LittleFs.cpp"
                            "src/MeasurementRecorder.cpp"
                            "src/Nvs.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core banana_hal esp_ringbuf log
                       PRIV_REQUIRES joltwallet__littlefs nvs_flash)
