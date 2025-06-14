#ifndef INC_I2C_LCD_H_
#define INC_I2C_LCD_H_

#include "stm32f4xx_hal.h"

void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_string(char *str);
void lcd_put_cur(int row, int col);
void lcd_clear(void);
void lcd_backlight_on(void);
void lcd_backlight_off(void);

#endif /* INC_I2C_LCD_H_ */
