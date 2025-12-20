#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

/**
 * @brief Initialize time manager with NTP sync
 */
esp_err_t time_manager_init(const char *ntp_server, const char *timezone);

/**
 * @brief Check if time is synchronized
 */
bool time_is_synced(void);

/**
 * @brief Get current time in ISO8601 format
 * @param buffer Output buffer for timestamp string
 * @param buffer_size Size of output buffer
 * @return ESP_OK on success
 */
esp_err_t time_get_iso8601(char *buffer, size_t buffer_size);

/**
 * @brief Force NTP sync
 */
esp_err_t time_force_sync(void);

#endif // TIME_MANAGER_H