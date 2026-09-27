# HomeAssistant.cpp is pure logic (host-tested); MqttService is the esp-mqtt client.
idf_component_register(SRCS "src/HomeAssistant.cpp"
                            "src/MqttService.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_config banana_control banana_core banana_hal banana_net espressif__mqtt
                       PRIV_REQUIRES bblanchon__arduinojson log)
