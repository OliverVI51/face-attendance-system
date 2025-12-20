#include "audio_task.h"
#include "mp3_driver.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_TASK";

extern mp3_handle_t g_mp3_handle;

void audio_task(void *pvParameters) {
    ESP_LOGI(TAG, "Audio task started");
    
    system_message_t msg;
    
    while (1) {
        if (xQueueReceive(g_audio_queue, &msg, portMAX_DELAY) == pdTRUE) {
            uint8_t track = 0;
            
            switch (msg.type) {
                case MSG_FINGERPRINT_MATCHED:
                    track = AUDIO_SUCCESS;
                    ESP_LOGI(TAG, "Playing success audio");
                    break;
                    
                case MSG_FINGERPRINT_NOT_MATCHED:
                case MSG_FINGERPRINT_TIMEOUT:
                case MSG_FINGERPRINT_ERROR:
                    track = AUDIO_FAILURE;
                    ESP_LOGI(TAG, "Playing failure audio");
                    break;
                    
                case MSG_PLAY_AUDIO:
                    track = msg.data.audio.track_number;
                    ESP_LOGI(TAG, "Playing audio track %d", track);
                    break;
                    
                default:
                    continue;
            }
            
            // Check if out of service
            EventBits_t bits = xEventGroupGetBits(g_system_events);
            if (bits & EVENT_OUT_OF_SERVICE) {
                // Only play out-of-service audio
                track = AUDIO_OUT_OF_SERVICE;
                ESP_LOGI(TAG, "System out of service, playing track 4");
            }
            
            if (track > 0) {
                mp3_play_track(g_mp3_handle, track);
            }
        }
    }
}