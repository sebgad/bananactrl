// Host stand-in for ESP-IDF's esp_err.h (banana::Result needs esp_err_t only).
#pragma once

typedef int esp_err_t; // NOLINT(modernize-use-using): C header

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108

inline const char* esp_err_to_name(esp_err_t /*code*/)
{
    return "esp_err";
}
