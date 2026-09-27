# Pure logic, no ESP-IDF dependencies apart from esp_err_t: also built on the host (test/host).
idf_component_register(SRCS "src/BrewFeedForward.cpp"
                            "src/ConfigMapping.cpp"
                            "src/ControlLoop.cpp"
                            "src/PidController.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_config banana_core)
