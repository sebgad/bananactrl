idf_component_register(SRCS "src/Ads1115.cpp"
                            "src/Ads1115Config.cpp"
                            "src/RgbLed.cpp"
                            "src/Ssr.cpp"
                            "src/TemperatureConverters.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_config banana_core banana_hal
                       PRIV_REQUIRES log)
