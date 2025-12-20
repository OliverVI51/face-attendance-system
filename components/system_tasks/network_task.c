#include "network_task.h"
#include "network_manager.h"
#include "time_manager.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "NETWORK_TASK";

static TickType_t last_server_check = 0;
static bool server_reachable = true;

void network_task(void *pvParameters) {
    ESP_LOGI(TAG, "Network task started");
    
    system_message_t msg;
    
    while (1) {
        // Check server reachability periodically
        TickType_t now = xTaskGetTickCount();
        if ((now - last_server_check) > pdMS_TO_TICKS(10000)) {  // Every 10 seconds
            last_server_check = now;
            
            EventBits_t bits = xEventGroupGetBits(g_system_events);
            if (bits & EVENT_WIFI_CONNECTED) {
                bool reachable = network_is_server_reachable(HTTP_SERVER_URL);
                
                if (reachable && !server_reachable) {
                    ESP_LOGI(TAG, "Server is now reachable");
                    server_reachable = true;
                    xEventGroupSetBits(g_system_events, EVENT_HTTP_AVAILABLE);
                    xEventGroupClearBits(g_system_events, EVENT_OUT_OF_SERVICE);
                } else if (!reachable && server_reachable) {
                    ESP_LOGW(TAG, "Server unreachable");
                    server_reachable = false;
                    xEventGroupClearBits(g_system_events, EVENT_HTTP_AVAILABLE);
                    
                    // Start out-of-service timer
                    vTaskDelay(pdMS_TO_TICKS(OUT_OF_SERVICE_TIMEOUT_SEC * 1000));
                    
                    // Check again
                    if (!network_is_server_reachable(HTTP_SERVER_URL)) {
                        ESP_LOGE(TAG, "Entering out-of-service mode");
                        xEventGroupSetBits(g_system_events, EVENT_OUT_OF_SERVICE);
                        
                        // Play out-of-service audio
                        system_message_t audio_msg = {
                            .type = MSG_PLAY_AUDIO,
                            .data.audio.track_number = AUDIO_OUT_OF_SERVICE
                        };
                        xQueueSend(g_audio_queue, &audio_msg, 0);
                    }
                }
            }
        }
        
        // Wait for HTTP POST request
        if (xQueueReceive(g_network_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (msg.type == MSG_FINGERPRINT_MATCHED) {
                ESP_LOGI(TAG, "Received fingerprint match, preparing HTTP POST");
                
                // Get current timestamp
                char timestamp[64];
                esp_err_t ret = time_get_iso8601(timestamp, sizeof(timestamp));
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to get timestamp");
                    continue;
                }
                
                // Build JSON payload
                char json_payload[256];
                snprintf(json_payload, sizeof(json_payload),
                        "{\"fingerprint_id\":%d,\"timestamp\":\"%s\",\"login_method\":\"fingerprint\"}",
                        msg.data.fingerprint.fingerprint_id,
                        timestamp);
                
                ESP_LOGI(TAG, "Sending HTTP POST: %s", json_payload);
                
                // Send HTTP POST with retries
                int retry = 0;
                bool success = false;
                
                while (retry < HTTP_RETRY_COUNT && !success) {
                    ret = network_http_post(HTTP_SERVER_URL, json_payload);
                    
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "HTTP POST successful");
                        success = true;
                        
                        system_message_t http_success_msg = {.type = MSG_HTTP_SUCCESS};
                        // Can notify other tasks if needed
                    } else {
                        ESP_LOGE(TAG, "HTTP POST failed, retry %d/%d", retry + 1, HTTP_RETRY_COUNT);
                        retry++;
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
                
                if (!success) {
                    ESP_LOGE(TAG, "HTTP POST failed after all retries");
                    system_message_t http_fail_msg = {.type = MSG_HTTP_FAILURE};
                    // Can cache for later retry if needed
                }
            }
        }
    }
}