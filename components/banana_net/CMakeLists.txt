idf_component_register(SRCS "src/MdnsService.cpp"
                            "src/TimeSync.cpp"
                            "src/WifiManager.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core banana_hal esp_event esp_netif
                       PRIV_REQUIRES espressif__mdns esp_wifi log lwip)
