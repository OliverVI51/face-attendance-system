#include "mp3_driver.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MP3_DRIVER";

struct mp3_driver {
    int uart_num;
    int tx_pin;
    int rx_pin;
    int baud_rate;
};
static struct mp3_driver g_mp3_dev; 

// Helper to calculate checksum and send
static void mp3_send_cmd(int uart_num, uint8_t cmd, uint16_t param) {
    // 7E FF 06 CMD 00 [ParamH] [ParamL] [ChkH] [ChkL] EF
    uint8_t param_h = (uint8_t)(param >> 8);
    uint8_t param_l = (uint8_t)(param & 0xFF);
    
    uint16_t sum = 0xFF + 0x06 + cmd + 0x00 + param_h + param_l;
    uint16_t checksum = 0 - sum;
    
    const uint8_t packet[] = {
        0x7E, 0xFF, 0x06, cmd, 0x00, param_h, param_l,
        (uint8_t)(checksum >> 8), (uint8_t)(checksum & 0xFF),
        0xEF
    };
    
    uart_write_bytes(uart_num, (const char*)packet, sizeof(packet));
}

esp_err_t mp3_init(const mp3_config_t *config, mp3_handle_t *handle) {
    if (!config || !handle) return ESP_ERR_INVALID_ARG;
    
    uart_config_t uart_cfg = {
        .baud_rate = config->baud_rate, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(config->uart_num, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(config->uart_num, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, -1, -1));

    g_mp3_dev.uart_num = config->uart_num;
    *handle = &g_mp3_dev;
    
    mp3_set_volume(*handle, config->volume);
    return ESP_OK;
}

esp_err_t mp3_set_volume(mp3_handle_t handle, uint8_t volume) {
    struct mp3_driver *dev = (struct mp3_driver *)handle;
    if (volume > 30) volume = 30;
    mp3_send_cmd(dev->uart_num, 0x06, volume); // 0x06 = Set Volume
    return ESP_OK;
}

esp_err_t mp3_play_track(mp3_handle_t handle, uint8_t track_num) {
    struct mp3_driver *dev = (struct mp3_driver *)handle;
    // 0x03 = Specify Tracking (0-2999)
    mp3_send_cmd(dev->uart_num, 0x03, track_num);
    return ESP_OK;
}

esp_err_t mp3_stop(mp3_handle_t handle) {
    struct mp3_driver *dev = (struct mp3_driver *)handle;
    // 0x16 = Stop
    mp3_send_cmd(dev->uart_num, 0x16, 0);
    return ESP_OK;
}

esp_err_t mp3_get_file_count(mp3_handle_t handle, uint16_t *count) {
    struct mp3_driver *dev = (struct mp3_driver *)handle;
    uart_flush_input(dev->uart_num);

    // 1. Check Online Status (0x3F)
    // 0 - (FF+06+3F) = 0 - 144 = FBBZ -> FE BC
    const uint8_t cmd_online[] = {0x7E, 0xFF, 0x06, 0x3F, 0x00, 0x00, 0x00, 0xFE, 0xBC, 0xEF};
    uart_write_bytes(dev->uart_num, (const char*)cmd_online, 10);
    uint8_t buf[10];
    int len = uart_read_bytes(dev->uart_num, buf, 10, pdMS_TO_TICKS(500));

    if (len == 10 && buf[3] == 0x3F) {
        if (!(buf[6] & 0x02)) { // SD Card bit check
            ESP_LOGW(TAG, "SD Card not detected");
            return ESP_ERR_NOT_FOUND;
        }
    } else {
        return ESP_ERR_TIMEOUT;
    }

    // 2. Query File Count (0x48)
    uart_flush_input(dev->uart_num);
    // 0 - (FF+06+48) = 0 - 14D = FBB3 -> FE B3
    const uint8_t cmd_count[] = {0x7E, 0xFF, 0x06, 0x48, 0x00, 0x00, 0x00, 0xFE, 0xB3, 0xEF};
    uart_write_bytes(dev->uart_num, (const char*)cmd_count, 10);
    len = uart_read_bytes(dev->uart_num, buf, 10, pdMS_TO_TICKS(500));

    if (len == 10 && buf[3] == 0x48) {
        *count = (buf[5] << 8) | buf[6];
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}