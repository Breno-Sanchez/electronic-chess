#ifndef LED_H
#define LED_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t ledInit(QueueHandle_t queue);
void ledTask(void * parameters);

#endif
