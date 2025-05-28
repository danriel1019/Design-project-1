#ifndef __DHT_H__
#define __DHT_H__

#include "stm32f4xx_hal.h"

// Define your DHT sensor's GPIO pin and port
#define DHT_PORT GPIOA
#define DHT_PIN  GPIO_PIN_1

// Timeout settings
#define DHT_TIMEOUT 10000

// Function to initialize DHT (optional)
void DHT_Init(void);

// Reads from the sensor, returns 0 if successful
int DHT_Read(uint8_t *humidity, uint8_t *temperature);

#endif /* __DHT_H__ */
