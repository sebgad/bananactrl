idf_component_register(SRCS "src/GpioInput.cpp"
                            "src/GpioOutput.cpp"
                            "src/I2cBus.cpp"
                            "src/I2cDevice.cpp"
                            "src/Task.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core esp_driver_gpio esp_driver_i2c freertos)
