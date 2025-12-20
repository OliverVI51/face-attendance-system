#include "keypad_driver.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "KEYPAD";

// 4x4 Keypad layout
static const char keypad_map[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

struct keypad_driver {
    int row_pins[4];
    int col_pins[4];
    uint32_t scan_interval_ms;
    esp_timer_handle_t scan_timer;
    keypad_callback_t callback;
    void *user_data;
    char last_key;
    bool enabled;
};

static void keypad_scan_timer_callback(void *arg) {
    keypad_handle_t handle = (keypad_handle_t)arg;
    
    if (!handle->enabled) {
        return;
    }
    
    // Scan each row
    for (int row = 0; row < 4; row++) {
        // Set current row LOW, others HIGH
        for (int r = 0; r < 4; r++) {
            gpio_set_level(handle->row_pins[r], (r == row) ? 0 : 1);
        }
        
        // Small delay for signal to settle
        esp_rom_delay_us(5);
        
        // Check columns
        for (int col = 0; col < 4; col++) {
            if (gpio_get_level(handle->col_pins[col]) == 0) {
                char key = keypad_map[row][col];
                
                // Debounce: only trigger if different from last key
                if (key != handle->last_key) {
                    handle->last_key = key;
                    
                    if (handle->callback) {
                        handle->callback(key, handle->user_data);
                    }
                    
                    ESP_LOGI(TAG, "Key pressed: %c", key);
                }
                return;
            }
        }
    }
    
    // No key pressed
    handle->last_key = 0;
    
    // Set all rows HIGH
    for (int r = 0; r < 4; r++) {
        gpio_set_level(handle->row_pins[r], 1);
    }
}

esp_err_t keypad_init(const keypad_config_t *config, keypad_handle_t *handle) {
    ESP_LOGI(TAG, "Initializing keypad driver");
    
    keypad_handle_t h = malloc(sizeof(struct keypad_driver));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    
    memcpy(h->row_pins, config->row_pins, sizeof(h->row_pins));
    memcpy(h->col_pins, config->col_pins, sizeof(h->col_pins));
    h->scan_interval_ms = config->scan_interval_ms;
    h->callback = NULL;
    h->user_data = NULL;
    h->last_key = 0;
    h->enabled = true;
    
    // Configure row pins as OUTPUT
    gpio_reset_pin(3);
    for (int i = 0; i < 4; i++) {
        gpio_config_t row_conf = {
            .pin_bit_mask = (1ULL << config->row_pins[i]),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&row_conf));
        gpio_set_level(config->row_pins[i], 1);
    }
    
    // Configure column pins as INPUT with pull-up
    for (int i = 0; i < 4; i++) {
        gpio_config_t col_conf = {
            .pin_bit_mask = (1ULL << config->col_pins[i]),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&col_conf));
    }
    
    // Create periodic timer for scanning
    esp_timer_create_args_t timer_args = {
        .callback = keypad_scan_timer_callback,
        .arg = h,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "keypad_scan"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &h->scan_timer));
    
    *handle = h;
    ESP_LOGI(TAG, "Keypad initialized");
    return ESP_OK;
}

esp_err_t keypad_register_callback(keypad_handle_t handle, keypad_callback_t callback, void *user_data) {
    handle->callback = callback;
    handle->user_data = user_data;
    return ESP_OK;
}

esp_err_t keypad_start(keypad_handle_t handle) {
    ESP_LOGI(TAG, "Starting keypad scanning");
    return esp_timer_start_periodic(handle->scan_timer, handle->scan_interval_ms * 1000);
}

esp_err_t keypad_stop(keypad_handle_t handle) {
    ESP_LOGI(TAG, "Stopping keypad scanning");
    return esp_timer_stop(handle->scan_timer);
}

esp_err_t keypad_set_enabled(keypad_handle_t handle, bool enabled) {
    handle->enabled = enabled;
    ESP_LOGI(TAG, "Keypad %s", enabled ? "enabled" : "disabled");
    return ESP_OK;
}