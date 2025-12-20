#include "mp3_driver.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "MP3_DRIVER";

#define MP3_START_BYTE 0x7E
#define MP3_END_BYTE 0xEF
#define MP3_VERSION 0xFF
#define MP3_LENGTH 0x06
#define MP3_FEEDBACK 0x00

#define MP3_RX_BUF_SIZE 128
#define MP3_TX_BUF_SIZE 128

struct mp3_driver {
    int uart_num;
    uint8_t volume;
};

// Send command to MP3 module
static esp_err_t send_command(mp3_handle_t handle, uint8_t cmd, uint8_t param1, uint8_t param2) {
    uint8_t packet[10];
    
    packet[0] = MP3_START_BYTE;
    packet[1] = MP3_VERSION;
    packet[2] = MP3_LENGTH;
    packet[3] = cmd;
    packet[4] = MP3_FEEDBACK;
    packet[5] = param1;
    packet[6] = param2;
    
    // Calculate checksum
    uint16_t checksum = 0;
    for (int i = 1; i < 7; i++) {
        checksum += packet[i];
    }
    checksum = 0 - checksum;
    
    packet[7] = (checksum >> 8) & 0xFF;
    packet[8] = checksum & 0xFF;
    packet[9] = MP3_END_BYTE;
    
    int written = uart_write_bytes(handle->uart_num, packet, 10);
    
    if (written != 10) {
        ESP_LOGE(TAG, "Failed to write command");
        return ESP_FAIL;
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));  // Small delay for command processing
    return ESP_OK;
}

esp_err_t mp3_init(const mp3_config_t *config, mp3_handle_t *handle) {
    ESP_LOGI(TAG, "Initializing MP3 player");
    
    mp3_handle_t h = malloc(sizeof(struct mp3_driver));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    
    h->uart_num = config->uart_num;
    h->volume = config->volume;
    
    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_driver_install(config->uart_num, MP3_RX_BUF_SIZE * 2, MP3_TX_BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(config->uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    vTaskDelay(pdMS_TO_TICKS(500));  // Wait for module to initialize
    
    // Select TF card
    send_command(h, MP3_CMD_SELECT_DEVICE, 0x00, 0x02);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Set initial volume
    send_command(h, MP3_CMD_SET_VOLUME, 0x00, config->volume);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    *handle = h;
    ESP_LOGI(TAG, "MP3 player initialized with volume %d", config->volume);
    return ESP_OK;
}

esp_err_t mp3_play_track(mp3_handle_t handle, uint8_t track) {
    ESP_LOGI(TAG, "Playing track %d", track);
    return send_command(handle, MP3_CMD_PLAY_TRACK, 0x00, track);
}

esp_err_t mp3_stop(mp3_handle_t handle) {
    ESP_LOGI(TAG, "Stopping playback");
    return send_command(handle, MP3_CMD_STOP, 0x00, 0x00);
}

esp_err_t mp3_set_volume(mp3_handle_t handle, uint8_t volume) {
    if (volume > 30) {
        volume = 30;
    }
    
    handle->volume = volume;
    ESP_LOGI(TAG, "Setting volume to %d", volume);
    return send_command(handle, MP3_CMD_SET_VOLUME, 0x00, volume);
}

esp_err_t mp3_reset(mp3_handle_t handle) {
    ESP_LOGI(TAG, "Resetting MP3 module");
    esp_err_t ret = send_command(handle, MP3_CMD_RESET, 0x00, 0x00);
    vTaskDelay(pdMS_TO_TICKS(500));
    return ret;
}

// Add to components/mp3_driver/mp3_driver.c

esp_err_t mp3_get_file_count(mp3_handle_t handle, uint16_t *count) {
    // Access internal UART num
    struct mp3_driver { int uart_num; /*...*/ }; 
    struct mp3_driver *dev = (struct mp3_driver *)handle;

    // Flush buffer
    uart_flush_input(dev->uart_num);

    // Command: 0x48 (Query TF Card file count)
    // Format: $S VER Len CMD Feedback Para1 Para2 Checksum $O
    // Hex: 7E FF 06 48 00 00 00 FE B3 EF
    const uint8_t cmd[] = {
        0x7E, 0xFF, 0x06, 0x48, 0x00, 0x00, 0x00, 0xFE, 0xB3, 0xEF
    };

    uart_write_bytes(dev->uart_num, (const char*)cmd, sizeof(cmd));

    // Response Format: 7E FF 06 48 00 [DataH] [DataL] [CheckH] [CheckL] EF
    uint8_t buf[10];
    int len = uart_read_bytes(dev->uart_num, buf, 10, pdMS_TO_TICKS(500));

    if (len == 10 && buf[0] == 0x7E && buf[3] == 0x48) {
        *count = (buf[5] << 8) | buf[6];
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}