idf_component_register(SRCS "src/Ads1115.cpp"
                            "src/Ads1115Config.cpp"
                            "src/TemperatureConverters.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core banana_hal)
