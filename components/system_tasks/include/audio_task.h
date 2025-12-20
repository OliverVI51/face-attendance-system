#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Audio task - handles MP3 playback
 */
void audio_task(void *pvParameters);

#endif // AUDIO_TASK_H