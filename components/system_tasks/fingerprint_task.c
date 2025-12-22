#include "fingerprint_task.h"
#include "fingerprint_driver.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "FP_TASK";
extern fingerprint_handle_t g_fingerprint_handle;

static esp_err_t get_image_and_convert(uint8_t buffer_id, int timeout_sec) {
    TickType_t start = xTaskGetTickCount();
    TickType_t end = start + pdMS_TO_TICKS(timeout_sec * 1000);
    while (xTaskGetTickCount() < end) {
        if (fingerprint_get_image(g_fingerprint_handle) == ESP_OK) {
            if (fingerprint_image_to_tz(g_fingerprint_handle, buffer_id) == ESP_OK) return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return ESP_ERR_TIMEOUT;
}

static void wait_finger_remove() {
    while (fingerprint_get_image(g_fingerprint_handle) == ESP_OK) vTaskDelay(pdMS_TO_TICKS(100));
}

void fingerprint_task(void *pvParameters) {
    ESP_LOGI(TAG, "Fingerprint task started");
    system_message_t msg;
    
    while (1) {
        if (xQueueReceive(g_fingerprint_queue, &msg, portMAX_DELAY) == pdTRUE) {
            EventBits_t bits = xEventGroupGetBits(g_system_events);

            if (msg.type == MSG_BUTTON_PRESSED) {
                if (bits & EVENT_OUT_OF_SERVICE) continue;
                if (!(bits & EVENT_NTP_SYNCED)) continue;
                
                g_current_state = STATE_FINGERPRINT_SCAN;
                system_message_t ui_msg = {.type = MSG_DISPLAY_UPDATE};
                xQueueSend(g_ui_queue, &ui_msg, 0);
                
                if (get_image_and_convert(1, FINGERPRINT_TIMEOUT_SEC) != ESP_OK) {
                    g_current_state = STATE_FAILURE;
                    system_message_t timeout_msg = {.type = MSG_FINGERPRINT_TIMEOUT};
                    xQueueSend(g_ui_queue, &timeout_msg, 0);
                    xQueueSend(g_audio_queue, &timeout_msg, 0);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE;
                    xQueueSend(g_ui_queue, &ui_msg, 0);
                    continue;
                }
                
                uint16_t fingerprint_id;
                uint16_t score;
                if (fingerprint_search(g_fingerprint_handle, &fingerprint_id, &score) == ESP_OK) {
                    g_current_state = STATE_SUCCESS;
                    system_message_t success_msg = { .type = MSG_FINGERPRINT_MATCHED, .data.fingerprint.fingerprint_id = fingerprint_id };
                    xQueueSend(g_ui_queue, &success_msg, 0);
                    xQueueSend(g_audio_queue, &success_msg, 0);
                    xQueueSend(g_network_queue, &success_msg, 0);
                } else {
                    g_current_state = STATE_FAILURE;
                    system_message_t fail_msg = {.type = MSG_FINGERPRINT_NOT_MATCHED};
                    xQueueSend(g_ui_queue, &fail_msg, 0);
                    xQueueSend(g_audio_queue, &fail_msg, 0);
                }
                vTaskDelay(pdMS_TO_TICKS(2000));
                g_current_state = STATE_IDLE;
                xQueueSend(g_ui_queue, &ui_msg, 0);
            }
            else if (msg.type == MSG_START_ENROLL) {
                uint16_t new_id = msg.data.enroll.enroll_id;
                system_message_t step1 = {.type = MSG_ENROLL_STEP_1};
                xQueueSend(g_ui_queue, &step1, 0);

                if (get_image_and_convert(1, 10) != ESP_OK) {
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0); continue;
                }
                wait_finger_remove();
                vTaskDelay(pdMS_TO_TICKS(500));

                system_message_t step2 = {.type = MSG_ENROLL_STEP_2};
                xQueueSend(g_ui_queue, &step2, 0);

                if (get_image_and_convert(2, 10) != ESP_OK) {
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0); continue;
                }

                if (fingerprint_create_model(g_fingerprint_handle) != ESP_OK || 
                    fingerprint_store_model(g_fingerprint_handle, new_id) != ESP_OK) {
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0); continue;
                }

                system_message_t success = { .type = MSG_ENROLL_SUCCESS, .data.enroll.enroll_id = new_id };
                xQueueSend(g_ui_queue, &success, 0);
                xQueueSend(g_audio_queue, &success, 0);
            }
            else if (msg.type == MSG_REQ_DELETE_USER) {
                uint16_t id = msg.data.fingerprint.fingerprint_id;
                esp_err_t ret = fingerprint_delete_model(g_fingerprint_handle, id);
                system_message_t res = { .type = MSG_DELETE_RESULT, .data.fingerprint.success = (ret == ESP_OK) };
                xQueueSend(g_ui_queue, &res, 0);
            }
        }
    }
}