idf_component_register(SRCS "app_main.cpp"
                            "App.cpp"
                            "HeaterTask.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES banana_config banana_control banana_core banana_drivers banana_hal banana_storage
                                esp_system esp_timer freertos log)
