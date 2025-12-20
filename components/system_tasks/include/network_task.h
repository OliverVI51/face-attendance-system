#ifndef NETWORK_TASK_H
#define NETWORK_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Network task - handles HTTP POST requests
 */
void network_task(void *pvParameters);

#endif // NETWORK_TASK_H