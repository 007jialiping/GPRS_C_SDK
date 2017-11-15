
#include <api_os.h>
#include <api_hal_gpio.h>
#include "demo_gpio.h"
#include "api_debug.h"

#define GPIO_Main_TASK_STACK_SIZE    (1024 * 2)
#define GPIO_Main_TASK_PRIORITY      1 
HANDLE gpioTaskHandle = NULL;

void Gpio_MainTask(VOID *pData)
{
    static GPIO_LEVEL ledBlueLevel = GPIO_LEVEL_LOW;
    GPIO_config_t gpioLedBlue = {
        .mode = GPIO_MODE_OUTPUT,
        .pin  = GPIO_PIN_LED_BLUE
    };

    GPIO_Init(gpioLedBlue);

    while(1)
    {
        ledBlueLevel = (ledBlueLevel==GPIO_LEVEL_HIGH)?GPIO_LEVEL_LOW:GPIO_LEVEL_HIGH;
        Trace(1,"gpio_main toggle:%d",ledBlueLevel);
        GPIO_SetLevel(gpioLedBlue,ledBlueLevel);   //Set level
        OS_Sleep(1000);                            //Sleep 1 s
    }
}
void gpio_Main(void)
{
    gpioTaskHandle = OS_CreateTask(Gpio_MainTask ,
        NULL, NULL, GPIO_Main_TASK_STACK_SIZE, GPIO_Main_TASK_PRIORITY, 0, 0, "main Task");
    OS_SetUserMainHandle(&gpioTaskHandle);
}