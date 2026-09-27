idf_component_register(SRCS "src/File.cpp"
                            "src/FileLogger.cpp"
                            "src/LittleFs.cpp"
                            "src/MeasurementRecorder.cpp"
                            "src/Nvs.cpp"
                            "src/NvsNamespace.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core banana_hal esp_ringbuf log nvs_flash
                       PRIV_REQUIRES joltwallet__littlefs)
