# Pure logic, no ESP-IDF dependencies: also built on the host (test/host).
idf_component_register(SRCS "src/PidController.cpp"
                            "src/BrewFeedForward.cpp"
                            "src/ConfigMapping.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_config)
