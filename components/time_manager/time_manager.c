#include "time_manager.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "TIME_MGR";

static bool s_time_synced = false;

static void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Time synchronized with NTP server");
    s_time_synced = true;
}

esp_err_t time_manager_init(const char *ntp_server, const char *timezone) {
    ESP_LOGI(TAG, "Initializing time manager");
    
    // Set timezone
    setenv("TZ", timezone, 1);
    tzset();
    
    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ntp_server);
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    
    ESP_LOGI(TAG, "Waiting for NTP sync...");
    
    // Wait for time sync (with timeout)
    int retry = 0;
    const int retry_count = 10;
    while (!s_time_synced && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    if (!s_time_synced) {
        ESP_LOGE(TAG, "Failed to sync time with NTP server");
        return ESP_ERR_TIMEOUT;
    }
    
    // Print current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Current local time: %s", strftime_buf);
    
    return ESP_OK;
}

bool time_is_synced(void) {
    return s_time_synced;
}

esp_err_t time_get_iso8601(char *buffer, size_t buffer_size) {
    if (!s_time_synced) {
        return ESP_ERR_INVALID_STATE;
    }
    
    time_t now;
    struct tm timeinfo;
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Format: 2025-12-18T14:30:00+02:00
    strftime(buffer, buffer_size, "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
    
    // Insert colon in timezone offset (e.g., +0200 -> +02:00)
    size_t len = strlen(buffer);
    if (len >= 2) {
        buffer[len + 1] = '\0';
        buffer[len] = buffer[len - 1];
        buffer[len - 1] = buffer[len - 2];
        buffer[len - 2] = ':';
    }
    
    return ESP_OK;
}

esp_err_t time_force_sync(void) {
    ESP_LOGI(TAG, "Forcing NTP sync");
    s_time_synced = false;
    esp_sntp_stop();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_sntp_init();
    
    // Wait for sync
    int retry = 0;
    while (!s_time_synced && retry++ < 10) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    return s_time_synced ? ESP_OK : ESP_ERR_TIMEOUT;
}