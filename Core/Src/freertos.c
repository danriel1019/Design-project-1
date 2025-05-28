#include "cmsis_os2.h"
#include "main.h"
#include "adc.h"
#include "gpio.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "dht.h"
#include "i2c-lcd.h"
#include <stdio.h>
#include <string.h>

/* Task handles */
osThreadId_t sensorTaskHandle;
osThreadId_t displayTaskHandle;

/* Shared sensor data */
uint8_t temp = 0, hum = 0, lightPercent = 0;

/* Task prototypes */
void SensorTask(void *argument);
void DisplayTask(void *argument);

/* FreeRTOS initialization */
void MX_FREERTOS_Init(void)
{
    const osThreadAttr_t sensorTask_attributes = {
        .name = "SensorTask",
        .stack_size = 128 * 4,
        .priority = (osPriority_t) osPriorityNormal,
    };
    sensorTaskHandle = osThreadNew(SensorTask, NULL, &sensorTask_attributes);

    const osThreadAttr_t displayTask_attributes = {
        .name = "DisplayTask",
        .stack_size = 128 * 4,
        .priority = (osPriority_t) osPriorityBelowNormal,
    };
    displayTaskHandle = osThreadNew(DisplayTask, NULL, &displayTask_attributes);
}

/* Task: Sensor + USB output */
void SensorTask(void *argument)
{
    MX_USB_DEVICE_Init();
    DHT_Init();

    char msg[64];
    uint32_t adcValue = 0;

    for (;;)
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK)
        {
            adcValue = HAL_ADC_GetValue(&hadc1);
            lightPercent = (adcValue * 100) / 4095;
        }
        HAL_ADC_Stop(&hadc1);

        if (DHT_Read(&hum, &temp) == 0)
        {
            snprintf(msg, sizeof(msg), "Light: %u%%, Temp: %dC, Hum: %d%%\r\n", lightPercent, temp, hum);
        }
        else
        {
            snprintf(msg, sizeof(msg), "Light: %u%%, DHT error\r\n", lightPercent);
        }

        CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
        osDelay(1000);
    }
}

/* Task: LCD display update */
void DisplayTask(void *argument)
{
    osDelay(100);  // Ensure power and I2C are stable
    lcd_init();
    lcd_clear();

    for (;;)
    {
        char line1[17];
        char line2[17];

        snprintf(line1, sizeof(line1), "T:%dC H:%d%%", temp, hum);
        snprintf(line2, sizeof(line2), "Light: %3d%%", lightPercent);

        lcd_put_cur(0, 0);
        lcd_send_string("                ");
        lcd_put_cur(0, 0);
        lcd_send_string(line1);

        lcd_put_cur(1, 0);
        lcd_send_string("                ");
        lcd_put_cur(1, 0);
        lcd_send_string(line2);

        osDelay(1000);
    }
}
