#include "fingerprint_task.h"
#include "fingerprint_driver.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "FP_TASK";
extern fingerprint_handle_t g_fingerprint_handle;

// Helper: Try to get image and convert it to char buffer (1 or 2)
static esp_err_t get_image_and_convert(uint8_t buffer_id, int timeout_sec) {
    TickType_t start = xTaskGetTickCount();
    TickType_t end = start + pdMS_TO_TICKS(timeout_sec * 1000);
    
    while (xTaskGetTickCount() < end) {
        if (fingerprint_get_image(g_fingerprint_handle) == ESP_OK) {
            // Got image, now convert
            if (fingerprint_image_to_tz(g_fingerprint_handle, buffer_id) == ESP_OK) {
                return ESP_OK;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return ESP_ERR_TIMEOUT;
}

// Helper: Wait for finger to be removed from sensor
static void wait_finger_remove() {
    while (fingerprint_get_image(g_fingerprint_handle) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void fingerprint_task(void *pvParameters) {
    ESP_LOGI(TAG, "Fingerprint task started");
    system_message_t msg;
    
    while (1) {
        // Wait for ANY message in the queue
        if (xQueueReceive(g_fingerprint_queue, &msg, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Received message type: %d", msg.type);
            
            EventBits_t bits = xEventGroupGetBits(g_system_events);

            // ==========================================
            // CASE 1: ATTENDANCE (Button Pressed)
            // ==========================================
            if (msg.type == MSG_BUTTON_PRESSED) {
                
                // 1. Check Service State
                if (bits & EVENT_OUT_OF_SERVICE) {
                    ESP_LOGW(TAG, "System out of service, ignoring.");
                    continue;
                }
                
                // 2. Check NTP
                if (!(bits & EVENT_NTP_SYNCED)) {
                    ESP_LOGE(TAG, "NTP not synced, blocking.");
                    // Optional: Send error audio
                    continue;
                }
                
                // 3. Update State & UI
                g_current_state = STATE_FINGERPRINT_SCAN;
                system_message_t ui_msg = {.type = MSG_DISPLAY_UPDATE};
                xQueueSend(g_ui_queue, &ui_msg, 0);
                
                // 4. Scan for Finger
                if (get_image_and_convert(1, FINGERPRINT_TIMEOUT_SEC) != ESP_OK) {
                    // Timeout or Error
                    g_current_state = STATE_FAILURE;
                    system_message_t timeout_msg = {.type = MSG_FINGERPRINT_TIMEOUT};
                    xQueueSend(g_ui_queue, &timeout_msg, 0);
                    xQueueSend(g_audio_queue, &timeout_msg, 0);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE;
                    xQueueSend(g_ui_queue, &ui_msg, 0);
                    continue;
                }
                
                // 5. Search Database
                uint16_t fingerprint_id;
                uint16_t score;
                esp_err_t ret = fingerprint_search(g_fingerprint_handle, &fingerprint_id, &score);
                
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Match Found: ID=%d", fingerprint_id);
                    g_current_state = STATE_SUCCESS;
                    
                    system_message_t success_msg = {
                        .type = MSG_FINGERPRINT_MATCHED,
                        .data.fingerprint.fingerprint_id = fingerprint_id
                    };
                    xQueueSend(g_ui_queue, &success_msg, 0);
                    xQueueSend(g_audio_queue, &success_msg, 0);
                    xQueueSend(g_network_queue, &success_msg, 0);
                } else {
                    ESP_LOGW(TAG, "No Match Found");
                    g_current_state = STATE_FAILURE;
                    system_message_t fail_msg = {.type = MSG_FINGERPRINT_NOT_MATCHED};
                    xQueueSend(g_ui_queue, &fail_msg, 0);
                    xQueueSend(g_audio_queue, &fail_msg, 0);
                }
                
                // Reset to Idle
                vTaskDelay(pdMS_TO_TICKS(2000));
                g_current_state = STATE_IDLE;
                xQueueSend(g_ui_queue, &ui_msg, 0);
            }

            // ==========================================
            // CASE 2: REGISTRATION (Enrollment)
            // ==========================================
            else if (msg.type == MSG_START_ENROLL) {
                uint16_t new_id = msg.data.enroll.enroll_id;
                ESP_LOGI(TAG, "Starting Enrollment for ID: %d", new_id);

                // --- Step 1: First Scan ---
                system_message_t step1_msg = {.type = MSG_ENROLL_STEP_1};
                xQueueSend(g_ui_queue, &step1_msg, 0);

                if (get_image_and_convert(1, 10) != ESP_OK) {
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0);
                    continue; // Abort
                }

                // Wait for finger to be lifted
                wait_finger_remove();
                vTaskDelay(pdMS_TO_TICKS(500));

                // --- Step 2: Second Scan ---
                system_message_t step2_msg = {.type = MSG_ENROLL_STEP_2};
                xQueueSend(g_ui_queue, &step2_msg, 0);

                if (get_image_and_convert(2, 10) != ESP_OK) {
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0);
                    continue; // Abort
                }

                // --- Step 3: Create Model (Combine features) ---
                if (fingerprint_create_model(g_fingerprint_handle) != ESP_OK) {
                    ESP_LOGE(TAG, "Model creation failed (fingers didn't match?)");
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0);
                    continue;
                }

                // --- Step 4: Store to Flash ---
                if (fingerprint_store_model(g_fingerprint_handle, new_id) != ESP_OK) {
                    ESP_LOGE(TAG, "Store model failed");
                    system_message_t fail = {.type = MSG_ENROLL_FAIL};
                    xQueueSend(g_ui_queue, &fail, 0);
                    continue;
                }

                // --- Success ---
                ESP_LOGI(TAG, "Enrollment Success for ID: %d", new_id);
                system_message_t success = {
                    .type = MSG_ENROLL_SUCCESS,
                    .data.enroll.enroll_id = new_id
                };
                xQueueSend(g_ui_queue, &success, 0);
                xQueueSend(g_audio_queue, &success, 0); // Beep on success
            }
            
            // ==========================================
            // CASE 3: DELETE USER (New)
            // ==========================================
            else if (msg.type == MSG_REQ_DELETE_USER) {
                // Get the ID from the request
                // In system_state.h we added MSG_REQ_DELETE_USER, generally passing ID via fingerprint.fingerprint_id 
                // or similar struct. Assuming data.fingerprint.fingerprint_id is used based on your UI logic.
                uint16_t id_to_delete = msg.data.fingerprint.fingerprint_id;
                
                ESP_LOGI(TAG, "Processing Delete Request for ID: %d", id_to_delete);
                
                // Call driver to delete
                esp_err_t ret = fingerprint_delete_model(g_fingerprint_handle, id_to_delete);
                
                // Construct result message
                system_message_t result_msg = {
                    .type = MSG_DELETE_RESULT,
                    .data.fingerprint.success = (ret == ESP_OK),
                    .data.fingerprint.fingerprint_id = id_to_delete
                };
                
                // Send result back to UI Task
                xQueueSend(g_ui_queue, &result_msg, 0);
                
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "Delete Successful");
                } else {
                    ESP_LOGE(TAG, "Delete Failed");
                }
            }
        }
    }
}