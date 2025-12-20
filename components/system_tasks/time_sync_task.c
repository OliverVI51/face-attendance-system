#include "time_sync_task.h"
#include "time_manager.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "TIME_SYNC_TASK";

void time_sync_task(void *pvParameters) {
    ESP_LOGI(TAG, "Time sync task started");
    
    // Wait for initial time sync (handled by time_manager_init)
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Check initial sync status
    if (time_is_synced()) {
        ESP_LOGI(TAG, "Initial time sync successful");
        xEventGroupSetBits(g_system_events, EVENT_NTP_SYNCED);
    } else {
        ESP_LOGE(TAG, "Initial time sync failed");
        xEventGroupClearBits(g_system_events, EVENT_NTP_SYNCED);
    }
    
    while (1) {
        // Periodic re-sync
        vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_INTERVAL_SEC * 1000));
        
        ESP_LOGI(TAG, "Performing periodic NTP sync");
        
        EventBits_t bits = xEventGroupGetBits(g_system_events);
        if (bits & EVENT_WIFI_CONNECTED) {
            esp_err_t ret = time_force_sync();
            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Periodic time sync successful");
                xEventGroupSetBits(g_system_events, EVENT_NTP_SYNCED);
            } else {
                ESP_LOGE(TAG, "Periodic time sync failed");
                xEventGroupClearBits(g_system_events, EVENT_NTP_SYNCED);
            }
        } else {
            ESP_LOGW(TAG, "Wi-Fi not connected, skipping time sync");
        }
    }
}