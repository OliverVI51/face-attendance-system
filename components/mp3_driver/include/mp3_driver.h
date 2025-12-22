#ifndef MP3_DRIVER_H
#define MP3_DRIVER_H

#include "esp_err.h"
#include <stdint.h>

// MP3 Config Structure
typedef struct {
    int uart_num;
    int tx_pin;
    int rx_pin;
    int baud_rate;
    uint8_t volume;
} mp3_config_t;

typedef struct mp3_driver* mp3_handle_t;

/**
 * @brief Initialize MP3 player
 */
esp_err_t mp3_init(const mp3_config_t *config, mp3_handle_t *handle);

/**
 * @brief Query number of files (and check if SD is present)
 */
esp_err_t mp3_get_file_count(mp3_handle_t handle, uint16_t *count);

/**
 * @brief Set volume (0-30)
 */
esp_err_t mp3_set_volume(mp3_handle_t handle, uint8_t volume);

/**
 * @brief Play specific track by index (0001.mp3 = 1)
 */
esp_err_t mp3_play_track(mp3_handle_t handle, uint8_t track_num);

/**
 * @brief Stop playback
 */
esp_err_t mp3_stop(mp3_handle_t handle);

#endif // MP3_DRIVER_H