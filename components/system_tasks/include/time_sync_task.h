#ifndef TIME_SYNC_TASK_H
#define TIME_SYNC_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Time sync task - periodically syncs time with NTP
 */
void time_sync_task(void *pvParameters);

#endif // TIME_SYNC_TASK_H