#ifndef FINGERPRINT_DRIVER_H
#define FINGERPRINT_DRIVER_H

#include "esp_err.h"
#include <stdint.h>

// R307S Command Codes
#define FP_CMD_GETIMAGE         0x01
#define FP_CMD_IMAGE2TZ         0x02
#define FP_CMD_MATCH            0x03
#define FP_CMD_SEARCH           0x04
#define FP_CMD_REGMODEL         0x05
#define FP_CMD_STORE            0x06
#define FP_CMD_LOAD             0x07
#define FP_CMD_UPCHAR           0x08
#define FP_CMD_DOWNCHAR         0x09
#define FP_CMD_UPLOADIMAGE      0x0A
#define FP_CMD_DOWNLOADIMAGE    0x0B
#define FP_CMD_DELETE           0x0C
#define FP_CMD_EMPTY            0x0D
#define FP_CMD_SETSYSPARAM      0x0E
#define FP_CMD_READSYSPARAM     0x0F
#define FP_CMD_TEMPLATECOUNT    0x1D

// Confirmation Codes
#define FP_OK                   0x00
#define FP_ERROR_RECV           0x01
#define FP_NO_FINGER            0x02
#define FP_IMAGEFAIL            0x03
#define FP_IMAGEMESS            0x06
#define FP_FEATUREFAIL          0x07
#define FP_NOMATCH              0x08
#define FP_NOTFOUND             0x09
#define FP_ENROLLMISMATCH       0x0A
#define FP_BADLOCATION          0x0B
#define FP_DBDELFAIL            0x10
#define FP_DBCLEARFAIL          0x11
#define FP_PASSFAIL             0x13
#define FP_INVALIDIMAGE         0x15
#define FP_FLASH_ERR            0x18
#define FP_INVALIDREG           0x1A
#define FP_ADDRCODE             0x20
#define FP_PASSVERIFY           0x21

// Fingerprint Driver Configuration
typedef struct {
    int uart_num;
    int tx_pin;
    int rx_pin;
    int baud_rate;
    uint32_t address;  // Default: 0xFFFFFFFF
} fingerprint_config_t;

// Fingerprint Driver Handle
typedef struct fingerprint_driver* fingerprint_handle_t;

/**
 * @brief Initialize fingerprint sensor
 */
esp_err_t fingerprint_init(const fingerprint_config_t *config, fingerprint_handle_t *handle);

/**
 * @brief Capture fingerprint image
 */
esp_err_t fingerprint_get_image(fingerprint_handle_t handle);

/**
 * @brief Convert image to template (buffer 1 or 2)
 */
esp_err_t fingerprint_image_to_tz(fingerprint_handle_t handle, uint8_t buffer_id);

/**
 * @brief Search for fingerprint in database
 * @return ESP_OK if found, fingerprint_id populated
 */
esp_err_t fingerprint_search(fingerprint_handle_t handle, uint16_t *fingerprint_id, uint16_t *score);

/**
 * @brief Create model from two templates
 */
esp_err_t fingerprint_create_model(fingerprint_handle_t handle);

/**
 * @brief Store model to flash
 */
esp_err_t fingerprint_store_model(fingerprint_handle_t handle, uint16_t location);

/**
 * @brief Get template count
 */
esp_err_t fingerprint_get_template_count(fingerprint_handle_t handle, uint16_t *count);

/**
 * @brief Delete specific template
 */
esp_err_t fingerprint_delete_model(fingerprint_handle_t handle, uint16_t location);

/**
 * @brief Empty database
 */
esp_err_t fingerprint_empty_database(fingerprint_handle_t handle);

/**
 * @brief Self-test: Checks if sensor is connected and communicating
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t fingerprint_self_test(fingerprint_handle_t handle);

#endif // FINGERPRINT_DRIVER_H