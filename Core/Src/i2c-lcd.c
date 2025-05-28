#include "i2c-lcd.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

#define SLAVE_ADDRESS_LCD 0x4E
extern I2C_HandleTypeDef hi2c1;

static void lcd_send(uint8_t data, uint8_t rs);

void lcd_init(void)
{
    HAL_Delay(50);
    lcd_send_cmd(0x30);
    HAL_Delay(5);
    lcd_send_cmd(0x30);
    HAL_Delay(1);
    lcd_send_cmd(0x30);
    lcd_send_cmd(0x20);  // 4-bit mode
    HAL_Delay(1);

    lcd_send_cmd(0x28);  // 2 line, 5x8 matrix
    lcd_send_cmd(0x08);  // display off
    lcd_send_cmd(0x01);  // clear display
    HAL_Delay(2);
    lcd_send_cmd(0x06);  // entry mode
    lcd_send_cmd(0x0C);  // display on, cursor off
}

void lcd_send_cmd(char cmd)
{
    uint8_t upper = cmd & 0xF0;
    uint8_t lower = (cmd << 4) & 0xF0;
    uint8_t data_t[4];

    data_t[0] = upper | 0x0C;
    data_t[1] = upper | 0x08;
    data_t[2] = lower | 0x0C;
    data_t[3] = lower | 0x08;

    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 4, HAL_MAX_DELAY);
}

void lcd_send_data(char data)
{
    uint8_t upper = data & 0xF0;
    uint8_t lower = (data << 4) & 0xF0;
    uint8_t data_t[4];

    data_t[0] = upper | 0x0D;
    data_t[1] = upper | 0x09;
    data_t[2] = lower | 0x0D;
    data_t[3] = lower | 0x09;

    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 4, HAL_MAX_DELAY);
}

void lcd_send_string(char *str)
{
    while (*str)
    {
        lcd_send_data(*str++);
    }
}

void lcd_put_cur(int row, int col)
{
    switch (row)
    {
    case 0:
        lcd_send_cmd(0x80 + col);
        break;
    case 1:
        lcd_send_cmd(0xC0 + col);
        break;
    }
}

void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_backlight_on(void)
{
    // No-op with this chip, but here for API consistency
}

void lcd_backlight_off(void)
{
    // Optional: could send command to turn off backlight (not always supported)
}
