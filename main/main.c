#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_common.h"

#include "app_config.h"
#include "system_state.h"
#include "fingerprint_driver.h"
#include "mp3_driver.h"
#include "display_driver.h"
#include "keypad_driver.h"
#include "network_manager.h"
#include "time_manager.h"
#include "ui_task.h"
#include "fingerprint_task.h"
#include "keypad_task.h"
#include "audio_task.h"
#include "network_task.h"
#include "time_sync_task.h"

static const char *TAG = "MAIN";

// --- Configuration Constants ---
#define BOOT_CHECK_MP3_COUNT    4
#define UI_LINE_HEIGHT          20
#define UI_START_Y              40
#define MP3_DEBUG_RETRY_COUNT   3

// --- Global Queue Handles ---
QueueHandle_t g_ui_queue;
QueueHandle_t g_fingerprint_queue;
QueueHandle_t g_keypad_queue;
QueueHandle_t g_audio_queue;
QueueHandle_t g_network_queue;

// --- Global Event Group ---
EventGroupHandle_t g_system_events;

// --- Driver Handles ---
fingerprint_handle_t g_fingerprint_handle;
mp3_handle_t g_mp3_handle;
display_handle_t g_display_handle;
keypad_handle_t g_keypad_handle;

// --- Callbacks ---

static void keypad_callback(char key, void *user_data) {
    system_message_t key_msg = {
        .type = MSG_KEYPAD_KEY_PRESSED,
        .data.keypad.key = key
    };
    // Non-blocking send
    xQueueSend(g_keypad_queue, &key_msg, 0);

    // Shortcut: Trigger Fingerprint Scan directly on 'A'
    if (key == 'A') {
        system_message_t fp_msg = { .type = MSG_BUTTON_PRESSED };
        xQueueSend(g_fingerprint_queue, &fp_msg, 0);
    }
}

static void network_event_callback(bool connected, void *user_data) {
    if (connected) {
        ESP_LOGI(TAG, "Wi-Fi connected");
        xEventGroupSetBits(g_system_events, EVENT_WIFI_CONNECTED);
        xEventGroupClearBits(g_system_events, EVENT_WIFI_DISCONNECTED);
    } else {
        ESP_LOGI(TAG, "Wi-Fi disconnected");
        xEventGroupClearBits(g_system_events, EVENT_WIFI_CONNECTED);
        xEventGroupSetBits(g_system_events, EVENT_WIFI_DISCONNECTED);
    }
}

// --- Main Application Entry ---

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Attendance System Starting...");
    
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 2. Create Synchronization Objects
    g_ui_queue = xQueueCreate(10, sizeof(system_message_t));
    g_fingerprint_queue = xQueueCreate(5, sizeof(system_message_t));
    g_keypad_queue = xQueueCreate(20, sizeof(system_message_t));
    g_audio_queue = xQueueCreate(10, sizeof(system_message_t));
    g_network_queue = xQueueCreate(10, sizeof(system_message_t));
    g_system_events = xEventGroupCreate();
    
    // 3. Initialize Display (First, so we can see errors)
    display_config_t display_config = {
        .mosi_pin = LCD_MOSI_PIN, .sclk_pin = LCD_SCLK_PIN, .cs_pin = LCD_CS_PIN,
        .dc_pin = LCD_DC_PIN, .rst_pin = LCD_RST_PIN, .bl_pin = LCD_BL_PIN,
        .spi_host = LCD_SPI_HOST, .h_res = LCD_H_RES, .v_res = LCD_V_RES,
        .pixel_clock_hz = LCD_PIXEL_CLOCK_HZ
    };
    ESP_ERROR_CHECK(display_init(&display_config, &g_display_handle));
    
    display_clear(g_display_handle, COLOR_BLACK);
    display_draw_text_large(g_display_handle, 10, 10, "BOOT DIAGNOSTIC", COLOR_WHITE, COLOR_BLACK);

    bool hardware_failed = false;
    int current_y = UI_START_Y + UI_LINE_HEIGHT;

    // 4. Network Check
    display_draw_text(g_display_handle, 10, current_y, "Network:", COLOR_WHITE, COLOR_BLACK);
    network_manager_register_callback(network_event_callback, NULL);
    ret = network_manager_init(WIFI_SSID, WIFI_PASSWORD);
    
    if (ret == ESP_OK && network_hardware_check() == ESP_OK) {
        display_draw_text(g_display_handle, 120, current_y, "[OK]", COLOR_GREEN, COLOR_BLACK);
    } else {
        display_draw_text(g_display_handle, 120, current_y, "[FAIL]", COLOR_RED, COLOR_BLACK);
        ESP_LOGE(TAG, "Network Hardware Failure");
        hardware_failed = true;
    }
    current_y += UI_LINE_HEIGHT;

    // 5. Fingerprint Check
    display_draw_text(g_display_handle, 10, current_y, "Fingerprint:", COLOR_WHITE, COLOR_BLACK);
    fingerprint_config_t fp_config = {
        .uart_num = FINGERPRINT_UART, .tx_pin = UART1_TX_PIN, .rx_pin = UART1_RX_PIN,
        .baud_rate = FINGERPRINT_BAUD, .address = 0xFFFFFFFF
    };
    ESP_ERROR_CHECK(fingerprint_init(&fp_config, &g_fingerprint_handle));

    if (fingerprint_self_test(g_fingerprint_handle) == ESP_OK) {
        display_draw_text(g_display_handle, 120, current_y, "[OK]", COLOR_GREEN, COLOR_BLACK);
    } else {
        display_draw_text(g_display_handle, 120, current_y, "[FAIL]", COLOR_RED, COLOR_BLACK);
        ESP_LOGE(TAG, "Fingerprint Critical Failure");
        hardware_failed = true;
    }
    current_y += UI_LINE_HEIGHT;

    // 6. MP3 DFPlayer Check (Robust & Non-Blocking)
    ESP_LOGI(TAG, "--- MP3 DIAGNOSIS ---");
    display_draw_text(g_display_handle, 10, current_y, "Audio Files:", COLOR_WHITE, COLOR_BLACK);

    mp3_config_t mp3_config = {
        .uart_num = MP3_UART, .tx_pin = UART2_TX_PIN, .rx_pin = UART2_RX_PIN,
        .baud_rate = MP3_BAUD, .volume = 30
    };
    ESP_ERROR_CHECK(mp3_init(&mp3_config, &g_mp3_handle));

    // Wait for module to wake up
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    bool mp3_ok = false;
    uint16_t file_count = 0;

    // Retry loop
    for (int i = 1; i <= MP3_DEBUG_RETRY_COUNT; i++) {
        esp_err_t mp3_res = mp3_get_file_count(g_mp3_handle, &file_count);
        
        if (mp3_res == ESP_OK) {
            if (file_count >= BOOT_CHECK_MP3_COUNT) {
                mp3_ok = true;
                break;
            } else {
                 ESP_LOGW(TAG, "MP3: Found %d files (Need %d)", file_count, BOOT_CHECK_MP3_COUNT);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (mp3_ok) {
        display_draw_text(g_display_handle, 120, current_y, "[OK]   ", COLOR_GREEN, COLOR_BLACK);
    } else {
        // Warning only, do not halt
        display_draw_text(g_display_handle, 120, current_y, "[N/A]  ", COLOR_ORANGE, COLOR_BLACK);
        ESP_LOGW(TAG, "Audio Check Failed. Proceeding without sound.");
    }
    current_y += UI_LINE_HEIGHT;

    // 7. Keypad Init
    keypad_config_t keypad_config = {
        .row_pins = {KEYPAD_ROW1_PIN, KEYPAD_ROW2_PIN, KEYPAD_ROW3_PIN, KEYPAD_ROW4_PIN},
        .col_pins = {KEYPAD_COL1_PIN, KEYPAD_COL2_PIN, KEYPAD_COL3_PIN, KEYPAD_COL4_PIN},
        .scan_interval_ms = KEYPAD_SCAN_INTERVAL_MS
    };
    ESP_ERROR_CHECK(keypad_init(&keypad_config, &g_keypad_handle));
    ESP_ERROR_CHECK(keypad_register_callback(g_keypad_handle, keypad_callback, NULL));
    ESP_ERROR_CHECK(keypad_start(g_keypad_handle));

    // 8. Result
    if (hardware_failed) {
        ESP_LOGE(TAG, "CRITICAL HARDWARE FAILURE. HALTING.");
        display_draw_text_large(g_display_handle, 10, current_y + 20, "BOOT ERROR", COLOR_RED, COLOR_BLACK);
        while(1) { vTaskDelay(100); }
    }

    display_draw_text(g_display_handle, 10, current_y + 10, "System Ready!", COLOR_GREEN, COLOR_BLACK);
    vTaskDelay(pdMS_TO_TICKS(1000));
    display_clear(g_display_handle, COLOR_BLACK);

    // 9. Time Manager
    ret = time_manager_init(NTP_SERVER, TIMEZONE);
    if (ret == ESP_OK) xEventGroupSetBits(g_system_events, EVENT_NTP_SYNCED);
    
    // 10. Start Tasks
    xTaskCreatePinnedToCore(ui_task, "ui_task", STACK_SIZE_UI_TASK, NULL, PRIORITY_UI_TASK, NULL, 0);
    xTaskCreatePinnedToCore(fingerprint_task, "fingerprint_task", STACK_SIZE_FINGERPRINT_TASK, NULL, PRIORITY_FINGERPRINT_TASK, NULL, 0);
    xTaskCreatePinnedToCore(keypad_task, "keypad_task", STACK_SIZE_KEYPAD_TASK, NULL, PRIORITY_KEYPAD_TASK, NULL, 1);
    
    // Only start audio task if hardware is OK
    if (mp3_ok) {
        xTaskCreatePinnedToCore(audio_task, "audio_task", STACK_SIZE_AUDIO_TASK, NULL, PRIORITY_AUDIO_TASK, NULL, 1);
    }
    
    xTaskCreatePinnedToCore(network_task, "network_task", STACK_SIZE_NETWORK_TASK, NULL, PRIORITY_NETWORK_TASK, NULL, 1);
    xTaskCreatePinnedToCore(time_sync_task, "time_sync_task", STACK_SIZE_TIME_SYNC_TASK, NULL, PRIORITY_TIME_SYNC_TASK, NULL, 1);
    
    ESP_LOGI(TAG, "System initialization complete.");
}