#include "keypad_task.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "KEYPAD_TASK";

void keypad_task(void *pvParameters) {
    ESP_LOGI(TAG, "Keypad task started");
    
    system_message_t msg;
    
    while (1) {
        // Wait for a key press message from the driver callback (in main.c)
        if (xQueueReceive(g_keypad_queue, &msg, portMAX_DELAY) == pdTRUE) {
            
            // Check if out of service (Hardware lockout only)
            EventBits_t bits = xEventGroupGetBits(g_system_events);
            if (bits & EVENT_OUT_OF_SERVICE) {
                ESP_LOGW(TAG, "System out of service, ignoring keypad");
                continue;
            }

            ESP_LOGI(TAG, "Key processed: %c", msg.data.keypad.key);

            // 1. Forward to UI Queue
            // The UI Task contains the Logic (PIN check, Registration ID, etc.)
            xQueueSend(g_ui_queue, &msg, 0);

            // 2. Trigger Beep Sound
            system_message_t audio_msg = {
                .type = MSG_PLAY_AUDIO,
                .data.audio.track_number = AUDIO_SUCCESS 
            };
            xQueueSend(g_audio_queue, &audio_msg, 0);
        }
    }
}