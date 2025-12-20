#ifndef UI_TASK_H
#define UI_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief UI task - handles display updates
 */
void ui_task(void *pvParameters);

#endif // UI_TASK_H