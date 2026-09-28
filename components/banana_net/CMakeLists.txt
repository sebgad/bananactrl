idf_component_register(SRCS "src/HttpClient.cpp"
                            "src/MdnsService.cpp"
                            "src/TimeSync.cpp"
                            "src/WifiManager.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core banana_hal esp_event esp_http_client esp_netif
                       PRIV_REQUIRES espressif__mdns esp_wifi log lwip mbedtls)
