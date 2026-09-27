idf_component_register(SRCS "src/EventGroup.cpp"
                            "src/GpioInput.cpp"
                            "src/GpioOutput.cpp"
                            "src/I2cBus.cpp"
                            "src/I2cDevice.cpp"
                            "src/Ledc.cpp"
                            "src/PeriodicTimer.cpp"
                            "src/Restart.cpp"
                            "src/Task.cpp"
                            "src/Watchdog.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_system esp_timer
                                freertos
                       PRIV_REQUIRES log)
