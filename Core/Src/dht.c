#include "dht.h"
#include <stdint.h>    // Required for uint8_t
#include "stm32f4xx_hal.h"

static void DHT_Delay(uint16_t time)
{
    // Microsecond delay using DWT cycle counter
    uint32_t tickstart = DWT->CYCCNT;
    uint32_t ticks = time * (HAL_RCC_GetHCLKFreq() / 1000000);
    while ((DWT->CYCCNT - tickstart) < ticks);
}

void DHT_Init(void)
{
    // Enable DWT Cycle Counter for microsecond precision
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

int DHT_Read(uint8_t *humidity, uint8_t *temperature)
{
    uint8_t data[5] = {0};
    uint32_t timeout;

    // Prepare GPIO pin as output
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);

    // Start signal
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
    HAL_Delay(18);
    HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);
    DHT_Delay(20);

    // Prepare GPIO pin as input
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT_PORT, &GPIO_InitStruct);

    // Wait for DHT response
    timeout = DHT_TIMEOUT;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET && timeout--);
    if (timeout == 0) return 1;

    timeout = DHT_TIMEOUT;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_RESET && timeout--);
    if (timeout == 0) return 2;

    timeout = DHT_TIMEOUT;
    while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET && timeout--);
    if (timeout == 0) return 3;

    // Read 40 bits
    for (int i = 0; i < 40; i++)
    {
        timeout = DHT_TIMEOUT;
        while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_RESET && timeout--);
        if (timeout == 0) return 4;

        DHT_Delay(40); // wait ~40us

        if (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
            data[i / 8] |= (1 << (7 - (i % 8)));

        timeout = DHT_TIMEOUT;
        while (HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET && timeout--);
        if (timeout == 0) return 5;
    }

    // Verify checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
        return 6;

    // Convert raw data to actual values for DHT22
    uint16_t rawHumidity = (data[0] << 8) | data[1];
    uint16_t rawTemperature = (data[2] << 8) | data[3];

    float fHumidity = rawHumidity / 10.0;
    float fTemperature = (rawTemperature & 0x8000) ? -((rawTemperature & 0x7FFF) / 10.0) : (rawTemperature / 10.0);

    // Output as integer (you can change this if you want decimal places)
    *humidity = (uint8_t)fHumidity;
    *temperature = (uint8_t)fTemperature;

    return 0; // Success
}
