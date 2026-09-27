idf_component_register(SRCS "app_main.cpp"
                            "App.cpp"
                            "SensorMonitor.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES banana_core banana_hal banana_drivers banana_storage esp_system esp_timer
                                freertos log)
