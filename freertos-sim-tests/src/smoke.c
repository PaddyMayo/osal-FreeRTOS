/*
 * File:  smoke.c
 *
 * Purpose:
 *   Smoke test firmware. It runs on the BSP unchanged, as the application the
 *   BSP's startup task calls, and checks what a port must get right before
 *   anything else works: the tick advances, a task can be created, and a
 *   queue passes a message between two tasks.
 *
 *   It has no console to report on. It sets the exit status the BSP ends with,
 *   and the harness reads it where the BSP ends the application, see
 *   gdb/run_firmware_test.py.
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "osapi-bsp.h"
#include "osapi-common.h"
#include "osapi-error.h"

#define SMOKE_MESSAGE         0xC0FFEEU
#define SMOKE_DELAY_MS        20U
#define SMOKE_TIMEOUT_MS      1000U
#define SMOKE_QUEUE_LENGTH    4U
#define SMOKE_SENDER_PRIORITY ((configMAX_PRIORITIES / 2) + 1)

// The parameter must stay non-const to match FreeRTOS's TaskFunction_t signature
// cppcheck-suppress constParameterCallback
static void Smoke_SenderTask(void *Parameter)
{
    QueueHandle_t Queue   = (QueueHandle_t)Parameter;
    uint32_t      Message = SMOKE_MESSAGE;

    (void)xQueueSend(Queue, &Message, portMAX_DELAY);

    vTaskDelete(NULL);
}

static bool Smoke_TickAdvances(void)
{
    TickType_t Start = xTaskGetTickCount();

    vTaskDelay(pdMS_TO_TICKS(SMOKE_DELAY_MS));

    return (xTaskGetTickCount() - Start) >= pdMS_TO_TICKS(SMOKE_DELAY_MS);
}

static bool Smoke_QueuePassesMessage(void)
{
    QueueHandle_t Queue;
    uint32_t      Received = 0U;
    bool          Passed   = false;

    Queue = xQueueCreate(SMOKE_QUEUE_LENGTH, sizeof(uint32_t));
    if (Queue != NULL)
    {
        if (xTaskCreate(Smoke_SenderTask, "smoke_tx", configMINIMAL_STACK_SIZE, Queue, SMOKE_SENDER_PRIORITY, NULL)
            == pdPASS)
        {
            Passed = (xQueueReceive(Queue, &Received, pdMS_TO_TICKS(SMOKE_TIMEOUT_MS)) == pdPASS)
                     && (Received == SMOKE_MESSAGE);
        }

        vQueueDelete(Queue);
    }

    return Passed;
}

void OS_Application_Startup(void)
{
    bool Passed = Smoke_TickAdvances();

    Passed = Smoke_QueuePassesMessage() && Passed;

    OS_BSP_SetExitCode(Passed ? OS_SUCCESS : OS_ERROR);
}

/* Nothing runs in the background: returning ends the application */
void OS_Application_Run(void)
{
}
