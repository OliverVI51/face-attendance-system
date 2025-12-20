#ifndef KEYPAD_DRIVER_H
#define KEYPAD_DRIVER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// Keypad Configuration
typedef struct {
    int row_pins[4];
    int col_pins[4];
    uint32_t scan_interval_ms;
} keypad_config_t;

// Keypad Driver Handle
typedef struct keypad_driver* keypad_handle_t;

// Key callback function type
typedef void (*keypad_callback_t)(char key, void *user_data);

/**
 * @brief Initialize keypad driver
 */
esp_err_t keypad_init(const keypad_config_t *config, keypad_handle_t *handle);

/**
 * @brief Register callback for key press events
 */
esp_err_t keypad_register_callback(keypad_handle_t handle, keypad_callback_t callback, void *user_data);

/**
 * @brief Start keypad scanning
 */
esp_err_t keypad_start(keypad_handle_t handle);

/**
 * @brief Stop keypad scanning
 */
esp_err_t keypad_stop(keypad_handle_t handle);

/**
 * @brief Enable/disable keypad
 */
esp_err_t keypad_set_enabled(keypad_handle_t handle, bool enabled);

#endif // KEYPAD_DRIVER_H