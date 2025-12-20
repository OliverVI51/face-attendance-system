#ifndef FINGERPRINT_TASK_H
#define FINGERPRINT_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Fingerprint task - handles fingerprint scanning and registration
 */
void fingerprint_task(void *pvParameters);

#endif // FINGERPRINT_TASK_H