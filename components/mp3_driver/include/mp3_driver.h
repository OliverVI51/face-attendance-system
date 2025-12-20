#ifndef MP3_DRIVER_H
#define MP3_DRIVER_H

#include "esp_err.h"
#include <stdint.h>

// MP3-TF-16P Commands
#define MP3_CMD_PLAY_NEXT       0x01
#define MP3_CMD_PLAY_PREV       0x02
#define MP3_CMD_PLAY_TRACK      0x03
#define MP3_CMD_VOL_UP          0x04
#define MP3_CMD_VOL_DOWN        0x05
#define MP3_CMD_SET_VOLUME      0x06
#define MP3_CMD_SET_EQ          0x07
#define MP3_CMD_REPEAT_TRACK    0x08
#define MP3_CMD_SELECT_DEVICE   0x09
#define MP3_CMD_SLEEP           0x0A
#define MP3_CMD_RESET           0x0C
#define MP3_CMD_PLAY            0x0D
#define MP3_CMD_PAUSE           0x0E
#define MP3_CMD_PLAY_FOLDER     0x0F
#define MP3_CMD_STOP            0x16

// MP3 Driver Configuration
typedef struct {
    int uart_num;
    int tx_pin;
    int rx_pin;
    int baud_rate;
    uint8_t volume;  // 0-30
} mp3_config_t;

// MP3 Driver Handle
typedef struct mp3_driver* mp3_handle_t;

/**
 * @brief Initialize MP3 player
 */
esp_err_t mp3_init(const mp3_config_t *config, mp3_handle_t *handle);

/**
 * @brief Play specific track (1-255)
 */
esp_err_t mp3_play_track(mp3_handle_t handle, uint8_t track);

/**
 * @brief Stop playback
 */
esp_err_t mp3_stop(mp3_handle_t handle);

/**
 * @brief Set volume (0-30)
 */
esp_err_t mp3_set_volume(mp3_handle_t handle, uint8_t volume);

/**
 * @brief Reset module
 */
esp_err_t mp3_reset(mp3_handle_t handle);

/**
 * @brief Query the total number of files on the SD card
 * @param count Pointer to store the file count
 * @return ESP_OK if communication successful
 */
esp_err_t mp3_get_file_count(mp3_handle_t handle, uint16_t *count);

#endif // MP3_DRIVER_H