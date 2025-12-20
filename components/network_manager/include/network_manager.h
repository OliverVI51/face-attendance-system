#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

// Network event callback type
typedef void (*network_event_callback_t)(bool connected, void *user_data);

/**
 * @brief Initialize network manager (Wi-Fi + HTTP client)
 */
esp_err_t network_manager_init(const char *ssid, const char *password);

/**
 * @brief Register callback for network events
 */
esp_err_t network_manager_register_callback(network_event_callback_t callback, void *user_data);

/**
 * @brief Check if Wi-Fi is connected
 */
bool network_is_connected(void);

/**
 * @brief Send HTTP POST request with JSON payload
 * @param url Server URL
 * @param json_data JSON payload string
 * @return ESP_OK on success
 */
esp_err_t network_http_post(const char *url, const char *json_data);

/**
 * @brief Check if HTTP server is reachable
 */
bool network_is_server_reachable(const char *url);

/**
 * @brief Checks if WiFi hardware MAC is readable (Hardware Sanity Check)
 */
esp_err_t network_hardware_check(void);

#endif // NETWORK_MANAGER_H