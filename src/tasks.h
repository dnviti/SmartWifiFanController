#ifndef TASKS_H
#define TASKS_H

#include "config.h"

extern TaskHandle_t networkTaskHandle;
extern TaskHandle_t mainAppTaskHandle;

void networkTask(void *pvParameters);
void mainAppTask(void *pvParameters);

#endif // TASKS_H
