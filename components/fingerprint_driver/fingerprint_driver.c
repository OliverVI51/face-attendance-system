#include "fingerprint_driver.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "FP_DRIVER";

// Packet Constants
#define FP_STARTCODE 0xEF01
#define FP_DEFAULT_TIMEOUT_MS 1000
#define FP_RX_BUF_SIZE 256
#define FP_TX_BUF_SIZE 256

// Command Codes
#define FP_CMD_VFY_PWD      0x13
#define FP_CMD_HANDSHAKE    0x17 // Some models use this, but VFY_PWD is most common

struct fingerprint_driver {
    int uart_num;
    uint32_t address;
};

// --- Helper Functions ---

// Calculate checksum
static uint16_t calculate_checksum(const uint8_t *data, size_t len) {
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// Send packet to sensor
static esp_err_t send_packet(fingerprint_handle_t handle, uint8_t type, const uint8_t *data, uint16_t len) {
    uint8_t packet[FP_TX_BUF_SIZE];
    uint16_t idx = 0;

    // Start code
    packet[idx++] = (FP_STARTCODE >> 8) & 0xFF;
    packet[idx++] = FP_STARTCODE & 0xFF;

    // Address
    packet[idx++] = (handle->address >> 24) & 0xFF;
    packet[idx++] = (handle->address >> 16) & 0xFF;
    packet[idx++] = (handle->address >> 8) & 0xFF;
    packet[idx++] = handle->address & 0xFF;

    // Package identifier (0x01 = Command)
    packet[idx++] = type;

    // Package length (data + checksum)
    uint16_t pkg_len = len + 2;
    packet[idx++] = (pkg_len >> 8) & 0xFF;
    packet[idx++] = pkg_len & 0xFF;

    // Data
    if (data && len > 0) {
        memcpy(&packet[idx], data, len);
        idx += len;
    }

    // Checksum
    uint16_t checksum = calculate_checksum(&packet[6], len + 3);
    packet[idx++] = (checksum >> 8) & 0xFF;
    packet[idx++] = checksum & 0xFF;

    // Flush input before sending to ensure we don't read old garbage
    uart_flush_input(handle->uart_num);

    int written = uart_write_bytes(handle->uart_num, packet, idx);
    return (written == idx) ? ESP_OK : ESP_FAIL;
}

// Receive packet from sensor
static esp_err_t receive_packet(fingerprint_handle_t handle, uint8_t *type, uint8_t *data, uint16_t *len) {
    uint8_t buf[FP_RX_BUF_SIZE];
    
    // Read header first (9 bytes) to verify start code
    int received = uart_read_bytes(handle->uart_num, buf, 9, pdMS_TO_TICKS(FP_DEFAULT_TIMEOUT_MS));

    if (received < 9) {
        return ESP_ERR_TIMEOUT;
    }

    // Verify start code
    uint16_t start = (buf[0] << 8) | buf[1];
    if (start != FP_STARTCODE) {
        // Only log this if it's not a timeout, to avoid spamming logs on empty reads
        ESP_LOGW(TAG, "Invalid start code: 0x%04X", start);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Extract type
    *type = buf[6];

    // Extract length of remaining data
    uint16_t pkg_len = (buf[7] << 8) | buf[8];
    uint16_t data_len = pkg_len - 2; // Subtract checksum length

    // Read the rest of the packet (Data + Checksum)
    if (data_len + 2 > 0) {
        received = uart_read_bytes(handle->uart_num, &buf[9], data_len + 2, pdMS_TO_TICKS(100));
        if (received != data_len + 2) {
             return ESP_ERR_INVALID_RESPONSE;
        }
    }

    if (data_len > 0 && data) {
        memcpy(data, &buf[9], data_len);
    }
    if (len) {
        *len = data_len;
    }

    return ESP_OK;
}

// Send command and get response
static esp_err_t send_command(fingerprint_handle_t handle, uint8_t cmd, const uint8_t *params, uint16_t param_len, uint8_t *response, uint16_t *resp_len) {
    uint8_t cmd_data[FP_TX_BUF_SIZE];
    cmd_data[0] = cmd;

    if (params && param_len > 0) {
        memcpy(&cmd_data[1], params, param_len);
    }

    esp_err_t ret = send_packet(handle, 0x01, cmd_data, param_len + 1);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t type;
    return receive_packet(handle, &type, response, resp_len);
}

// Verify Hardware Connection (Handshake)
static esp_err_t fingerprint_check_connection(fingerprint_handle_t handle) {
    uint8_t params[4] = {0x00, 0x00, 0x00, 0x00}; // Password 0
    uint8_t response[32];
    uint16_t len;

    // Send Verify Password command
    esp_err_t ret = send_command(handle, FP_CMD_VFY_PWD, params, 4, response, &len);
    
    if (ret != ESP_OK) return ret;
    
    // confirmation code 0x00 means OK
    return (response[0] == 0x00) ? ESP_OK : ESP_FAIL;
}

// --- Public Functions ---

esp_err_t fingerprint_init(const fingerprint_config_t *config, fingerprint_handle_t *handle) {
    ESP_LOGI(TAG, "Initializing fingerprint sensor UART...");

    fingerprint_handle_t h = malloc(sizeof(struct fingerprint_driver));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }

    h->uart_num = config->uart_num;
    h->address = config->address;

    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(config->uart_num, FP_RX_BUF_SIZE * 2, FP_TX_BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(config->uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    *handle = h;

    // --- HARDWARE VERIFICATION ---
    ESP_LOGI(TAG, "Attempting to handshake with sensor...");
    bool sensor_found = false;
    
    // Try 3 times to connect
    for(int i = 0; i < 3; i++) {
        if(fingerprint_check_connection(h) == ESP_OK) {
            sensor_found = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (sensor_found) {
        ESP_LOGI(TAG, "Fingerprint sensor hardware FOUND and VERIFIED.");
    } else {
        ESP_LOGE(TAG, "Fingerprint sensor NOT RESPONDING."); 
        ESP_LOGE(TAG, "Please check wiring: ESP TX->Sensor RX (Green), ESP RX->Sensor TX (Yellow).");
        // We do NOT return ESP_FAIL here to prevent boot loop in app_main, 
        // but the sensor won't work until wiring is fixed.
    }

    return ESP_OK;
}

esp_err_t fingerprint_get_image(fingerprint_handle_t handle) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_GETIMAGE, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (response[0] == FP_OK) {
        return ESP_OK;
    } else if (response[0] == FP_NO_FINGER) {
        return ESP_ERR_NOT_FOUND;
    } else {
        ESP_LOGD(TAG, "Get Image Error: 0x%02X", response[0]);
    }

    return ESP_FAIL;
}

esp_err_t fingerprint_image_to_tz(fingerprint_handle_t handle, uint8_t buffer_id) {
    uint8_t params[1] = {buffer_id};
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_IMAGE2TZ, params, 1, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_search(fingerprint_handle_t handle, uint16_t *fingerprint_id, uint16_t *score) {
    // Search Buffer 1, Start ID 0, Count 200 (Expanded range)
    uint8_t params[5] = {0x01, 0x00, 0x00, 0x00, 0xC8}; 
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_SEARCH, params, 5, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (response[0] == FP_OK) {
        *fingerprint_id = (response[1] << 8) | response[2];
        *score = (response[3] << 8) | response[4];
        return ESP_OK;
    } else if (response[0] == FP_NOTFOUND) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_FAIL;
}

esp_err_t fingerprint_create_model(fingerprint_handle_t handle) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_REGMODEL, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_store_model(fingerprint_handle_t handle, uint16_t location) {
    uint8_t params[3] = {0x01, (location >> 8) & 0xFF, location & 0xFF};
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_STORE, params, 3, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_get_template_count(fingerprint_handle_t handle, uint16_t *count) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_TEMPLATECOUNT, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (response[0] == FP_OK) {
        *count = (response[1] << 8) | response[2];
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t fingerprint_delete_model(fingerprint_handle_t handle, uint16_t location) {
    uint8_t params[4] = {(location >> 8) & 0xFF, location & 0xFF, 0x00, 0x01};
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_DELETE, params, 4, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_empty_database(fingerprint_handle_t handle) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_EMPTY, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

// Add to components/fingerprint_driver/fingerprint_driver.c

esp_err_t fingerprint_self_test(fingerprint_handle_t handle) {
    // We send a "Read System Parameter" command (0x0F)
    // If the sensor is alive, it MUST reply with an acknowledgment packet.
    
    // 1. Send Command: READSYSPARAM (defined as 0x0F in your header)
    // You likely have a function like: write_packet(handle, FP_CMD_READSYSPARAM, ...);
    // If not, implementing the packet structure manually:
    
    /* 
       Implementation depends on your internal write_packet function. 
       Generic logic:
    */
    
    // Example call using your existing internal logic (assuming it exists):
    // esp_err_t err = send_command_packet(handle, FP_CMD_READSYSPARAM);
    // if (err != ESP_OK) return err;
    
    // receive_packet(handle, &reply_packet);
    // if (reply_packet.confirmation_code == FP_OK) return ESP_OK;
    
    // --- STANDALONE IMPLEMENTATION IF HELP NEEDED ---
    const uint8_t cmd_packet[] = {
        0xEF, 0x01,                         // Header
        0xFF, 0xFF, 0xFF, 0xFF,             // Address
        0x01,                               // Package Identifier (Command)
        0x00, 0x03,                         // Package Length (3 bytes)
        FP_CMD_READSYSPARAM,                // Command: Read Sys Param
        (uint8_t)(0x01 + 0x03 + FP_CMD_READSYSPARAM) >> 8, // Checksum High
        (uint8_t)(0x01 + 0x03 + FP_CMD_READSYSPARAM)       // Checksum Low
    };
    
    // Access the UART port from your handle (assuming struct definition)
    struct fingerprint_driver { int uart_num; /*...*/ }; // Cast handle if opaque
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;

    uart_write_bytes(dev->uart_num, (const char*)cmd_packet, sizeof(cmd_packet));
    
    // Wait for response header
    uint8_t buffer[12]; // Minimum ack packet size
    int len = uart_read_bytes(dev->uart_num, buffer, 12, pdMS_TO_TICKS(1000));
    
    if (len >= 9 && buffer[0] == 0xEF && buffer[1] == 0x01) {
        // buffer[6] is Package ID (0x07 for Ack), buffer[9] is Confirmation Code
        if (buffer[9] == 0x00) {
            return ESP_OK; // Sensor is alive and happy
        }
    }
    
    return ESP_FAIL;
}

// components/fingerprint_driver/fingerprint_driver.c

esp_err_t fingerprint_delete_model(fingerprint_handle_t handle, uint16_t location) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    
    // Flush input buffer
    uart_flush_input(dev->uart_num);

    // Calculate Checksum: PID + Len + CMD + ID_H + ID_L + N_H + N_L
    // CMD 0x0C = Delete
    // We delete 1 template (N=1)
    uint16_t checksum = 0x01 + 0x07 + 0x0C + (location >> 8) + (location & 0xFF) + 0x00 + 0x01;
    
    const uint8_t cmd[] = {
        0xEF, 0x01,                         // Header
        0xFF, 0xFF, 0xFF, 0xFF,             // Address
        0x01,                               // PID (Command)
        0x00, 0x07,                         // Length (7 bytes)
        0x0C,                               // Command: Delete Template
        (uint8_t)(location >> 8),           // PageID High
        (uint8_t)(location & 0xFF),         // PageID Low
        0x00, 0x01,                         // Number of templates to delete (1)
        (uint8_t)(checksum >> 8),           // Checksum High
        (uint8_t)(checksum & 0xFF)          // Checksum Low
    };

    uart_write_bytes(dev->uart_num, (const char*)cmd, sizeof(cmd));

    // Read Response
    uint8_t buf[12]; 
    int len = uart_read_bytes(dev->uart_num, buf, 12, pdMS_TO_TICKS(1000));

    // Check response: Header (EF 01) ... Confirmation Code at index 9
    if (len >= 10 && buf[0] == 0xEF && buf[9] == 0x00) {
        return ESP_OK;
    } else {
        ESP_LOGE("FP", "Delete Failed. Code: 0x%02x", (len >= 10) ? buf[9] : 0xFF);
        return ESP_FAIL;
    }
}