#include "cmsis_os2.h"
#include "main.h"
#include "adc.h"
#include "gpio.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "dht.h"
#include "i2c-lcd.h"
#include "i2c.h"
#include <stdio.h>
#include <string.h>

/* Thresholds */
#define TEMP_ALERT_HIGH     35
#define HUM_ALERT_HIGH      80
#define LIGHT_ALERT_LOW     10
#define LIGHT_ALERT_HIGH    90

/* Task handles */
osThreadId_t sensorTaskHandle;
osThreadId_t displayTaskHandle;
osThreadId_t buttonTaskHandle;

/* Queue handle */
osMessageQueueId_t sensorQueueHandle;

/* Display mode */
volatile uint8_t displayMode = 1;

/* Max recorded values */
uint8_t maxTemp = 0, maxHum = 0, maxLight = 0;

/* Struct for sensor readings */
typedef struct {
  uint8_t temp;
  uint8_t hum;
  uint8_t lightPercent;
  uint16_t rawTemp;
  uint16_t rawHum;
  uint16_t adcValue;
  uint8_t dhtStatus;
  uint8_t ldrStatus;
} SensorData_t;

/* Task prototypes */
void SensorTask(void *argument);
void DisplayTask(void *argument);
void ButtonTask(void *argument);

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

    const osThreadAttr_t buttonTask_attributes = {
        .name = "ButtonTask",
        .stack_size = 128 * 4,
        .priority = (osPriority_t) osPriorityLow,
    };
    buttonTaskHandle = osThreadNew(ButtonTask, NULL, &buttonTask_attributes);

    sensorQueueHandle = osMessageQueueNew(4, sizeof(SensorData_t), NULL);
}

/* Sensor task */
void SensorTask(void *argument)
{
    MX_USB_DEVICE_Init();
    DHT_Init();
    char msg[64];

    for (;;)
    {
        SensorData_t data;
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) == HAL_OK)
        {
            data.adcValue = HAL_ADC_GetValue(&hadc1);
            data.lightPercent = (data.adcValue * 100) / 4095;
            data.ldrStatus = (data.lightPercent > 1 && data.lightPercent < 99);
        }
        HAL_ADC_Stop(&hadc1);

        if (DHT_Read(&data.hum, &data.temp) == 0)
        {
            data.dhtStatus = 1;
            data.rawHum = data.hum * 10;
            data.rawTemp = data.temp * 10;

            if (data.temp > maxTemp) maxTemp = data.temp;
            if (data.hum > maxHum) maxHum = data.hum;
            if (data.lightPercent > maxLight) maxLight = data.lightPercent;

            snprintf(msg, sizeof(msg), "Temp: %dC  Hum: %d%%  Light: %d%%\r\n", data.temp, data.hum, data.lightPercent);
        }
        else
        {
            data.dhtStatus = 0;
            snprintf(msg, sizeof(msg), "DHT sensor error\r\n");
        }

        CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
        osMessageQueuePut(sensorQueueHandle, &data, 0, 0);
        osDelay(1000);
    }
}

/* Display task */
void DisplayTask(void *argument)
{
    osDelay(500);
    HAL_I2C_DeInit(&hi2c1);
    HAL_I2C_Init(&hi2c1);
    osDelay(10);
    lcd_init();
    osDelay(50);
    lcd_clear();
    osDelay(10);
    lcd_clear();
    osDelay(10);

    SensorData_t data;

    for (;;)
    {
        lcd_clear();

        char line1[17], line2[17], lbuf[14];
        char lcdAlert1[17] = "";
        char lcdAlert2[17] = "";
        uint8_t showAlert = 0;

        if (osMessageQueueGet(sensorQueueHandle, &data, NULL, 100) == osOK)
        {
            if (!data.dhtStatus) {
                snprintf(lcdAlert1, sizeof(lcdAlert1), "DHT SENSOR ERROR");
                CDC_Transmit_FS((uint8_t *)"ALERT: DHT SENSOR ERROR\r\n", strlen("ALERT: DHT SENSOR ERROR\r\n"));
                showAlert = 1;
            }
            else if (!data.ldrStatus) {
                snprintf(lcdAlert1, sizeof(lcdAlert1), "LDR SENSOR ERROR");
                CDC_Transmit_FS((uint8_t *)"ALERT: LDR SENSOR ERROR\r\n", strlen("ALERT: LDR SENSOR ERROR\r\n"));
                showAlert = 1;
            }
            else {
                if (data.temp > TEMP_ALERT_HIGH) {
                    snprintf(lcdAlert1, sizeof(lcdAlert1), "TEMP TOO HIGH");
                    CDC_Transmit_FS((uint8_t *)"ALERT: TEMP TOO HIGH\r\n", strlen("ALERT: TEMP TOO HIGH\r\n"));
                    showAlert = 1;
                }
                if (data.hum > HUM_ALERT_HIGH) {
                    if (showAlert && strlen(lcdAlert2) == 0)
                        snprintf(lcdAlert2, sizeof(lcdAlert2), "HUMIDITY TOO HIGH");
                    else if (!showAlert)
                        snprintf(lcdAlert1, sizeof(lcdAlert1), "HUMIDITY TOO HIGH");
                    CDC_Transmit_FS((uint8_t *)"ALERT: HUMIDITY TOO HIGH\r\n", strlen("ALERT: HUMIDITY TOO HIGH\r\n"));
                    showAlert = 1;
                }
                if (data.lightPercent < LIGHT_ALERT_LOW) {
                    if (showAlert && strlen(lcdAlert2) == 0)
                        snprintf(lcdAlert2, sizeof(lcdAlert2), "LIGHT LEVEL LOW");
                    else if (!showAlert)
                        snprintf(lcdAlert1, sizeof(lcdAlert1), "LIGHT LEVEL LOW");
                    CDC_Transmit_FS((uint8_t *)"ALERT: LIGHT LEVEL LOW\r\n", strlen("ALERT: LIGHT LEVEL LOW\r\n"));
                    showAlert = 1;
                }
                else if (data.lightPercent > LIGHT_ALERT_HIGH) {
                    if (showAlert && strlen(lcdAlert2) == 0)
                        snprintf(lcdAlert2, sizeof(lcdAlert2), "LIGHT LEVEL HIGH");
                    else if (!showAlert)
                        snprintf(lcdAlert1, sizeof(lcdAlert1), "LIGHT LEVEL HIGH");
                    CDC_Transmit_FS((uint8_t *)"ALERT: LIGHT LEVEL HIGH\r\n", strlen("ALERT: LIGHT LEVEL HIGH\r\n"));
                    showAlert = 1;
                }
            }

            if (showAlert)
            {
                lcd_clear();
                lcd_put_cur(0, 0); lcd_send_string(lcdAlert1);
                lcd_put_cur(1, 0); lcd_send_string(lcdAlert2[0] ? lcdAlert2 : "CHECK SYSTEM");
                osDelay(1000);
                lcd_clear();
            }

            if (displayMode == 1) {
                snprintf(line1, sizeof(line1), "T:%03u H:%03u", data.rawTemp, data.rawHum);
                snprintf(lbuf, sizeof(lbuf), "L:%lu", (unsigned long)data.adcValue);
                snprintf(line2, sizeof(line2), "%-13s[1]", lbuf);
            }
            else if (displayMode == 2) {
                snprintf(line1, sizeof(line1), "T:%02d\xDF""C H:%02d%%", data.temp, data.hum);
                snprintf(lbuf, sizeof(lbuf), "L:%d%%", data.lightPercent);
                snprintf(line2, sizeof(line2), "%-13s[2]", lbuf);
            }
            else if (displayMode == 3) {
                snprintf(line1, sizeof(line1), "T:%02d\xDF""C H:%02d%%", maxTemp, maxHum);
                snprintf(lbuf, sizeof(lbuf), "L:%d%%", maxLight);
                snprintf(line2, sizeof(line2), "%-13s[3]", lbuf);
            }
            else {
                snprintf(line1, sizeof(line1), "DHT:%s LDR:%s",
                         data.dhtStatus ? "OK" : "FAIL",
                         data.ldrStatus ? "OK" : "FAIL");
                snprintf(line2, sizeof(line2), "System Running");
            }

            lcd_put_cur(0, 0); lcd_send_string(line1);
            lcd_put_cur(1, 0); lcd_send_string(line2);
        }

        osDelay(1000);
    }
}

/* Button task */
void ButtonTask(void *argument)
{
    uint8_t lastButtonState = 1;
    for (;;)
    {
        uint8_t buttonState = HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
        if (lastButtonState == 1 && buttonState == 0)
        {
            displayMode++;
            if (displayMode > 4) displayMode = 1;
        }
        lastButtonState = buttonState;
        osDelay(50);
    }
}
