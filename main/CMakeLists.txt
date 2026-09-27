idf_component_register(SRCS "app_main.cpp"
                            "App.cpp"
                            "HeaterTask.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES banana_config banana_control banana_core banana_drivers banana_hal banana_mqtt banana_net banana_storage banana_web
                                esp_app_format esp_hw_support esp_system esp_timer freertos heap log)
