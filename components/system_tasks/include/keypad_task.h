#ifndef KEYPAD_TASK_H
#define KEYPAD_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Keypad task - handles keypad input and PIN entry
 */
void keypad_task(void *pvParameters);

#endif // KEYPAD_TASK_H