idf_component_register(SRCS "src/ApiRoutes.cpp"
                            "src/EventStream.cpp"
                            "src/LiveValues.cpp"
                            "src/OtaRoutes.cpp"
                            "src/OtaUpdater.cpp"
                            "src/StaticFileRoutes.cpp"
                            "src/WebServer.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES app_update banana_config banana_control banana_core banana_hal banana_net banana_storage esp_http_server esp_rom
                       PRIV_REQUIRES bblanchon__arduinojson esp_app_format esp_system freertos log)

# /failsafe: data/ota.html, gzip-compressed into the firmware (the page works without LittleFS)
idf_build_get_property(python PYTHON)
set(ota_html ${PROJECT_DIR}/data/ota.html)
set(ota_html_gz ${CMAKE_CURRENT_BINARY_DIR}/ota.html.gz)
add_custom_command(OUTPUT ${ota_html_gz}
                   COMMAND ${python} -c "import gzip, sys; open(sys.argv[2], 'wb').write(gzip.compress(open(sys.argv[1], 'rb').read(), 9, mtime=0))" ${ota_html} ${ota_html_gz}
                   DEPENDS ${ota_html}
                   VERBATIM)
add_custom_target(banana_web_failsafe DEPENDS ${ota_html_gz})
target_add_binary_data(${COMPONENT_LIB} ${ota_html_gz} BINARY DEPENDS banana_web_failsafe)
