#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "lcd.h"

void lcdSendComand(uint8_t comand)
{
    // whait until i1c is available for wirte

    unsigned char data_u, data_l;
    unsigned char data_t[4];

    data_u = (comand & 0xf0);        // obere 4 Bit des Kommandos
    data_l = ((comand << 4) & 0xf0); // untere 4 Bit des Kommandos

    data_t[0] = data_u | BL | E; // en=1, rs=0
    data_t[1] = data_u | BL;     // en=0, rs=0
    data_t[2] = data_l | BL | E; // en=1, rs=0
    data_t[3] = data_l | BL;     // en=0, rs=0
    i2c_write_blocking(i2c_default, LCD_ADR, data_t, 4, false);
}

void lcdSendData(char data)
{
    // whait until i1c is available for wirte
    while (!i2c_get_write_available(i2c_default))
        ;

    unsigned char data_u, data_l;
    unsigned char data_t[4];

    data_u = (data & 0xf0);        // obere 4 Bit des Kommandos
    data_l = ((data & 0x0f) << 4); // untere 4 Bit des Kommandos

    data_t[0] = data_u | BL | E | RS; // en=1, rs=1
    data_t[1] = data_u | BL | RS;     // en=0, rs=1
    data_t[2] = data_l | BL | E | RS; // en=1, rs=1
    data_t[3] = data_l | BL | RS;     // en=0, rs=1
    i2c_write_blocking(i2c_default, LCD_ADR, data_t, 4, false);
    busy_wait_us(53);
}

void lcdSendString(char *string)
{
    // sands char until \0
    while (*string)
    {
        lcdSendData(*string++);
    }
}

void lcdInit()
{
    busy_wait_ms(40);
    lcdSendComand(0x03);
    busy_wait_ms(5);
    lcdSendComand(0x03);
    busy_wait_ms(1);
    lcdSendComand(0x03);
    busy_wait_ms(1);
    lcdSendComand(0x02);

    busy_wait_ms(1);
    lcdSendComand(0x04 | 0x04);
    lcdSendComand(0x20 | 0x08);
    lcdSendComand(0x08 | 0x04);

    lcdClear();
}

void lcdClear(void)
{
    lcdSendComand(0x01); // clear LCD
    busy_wait_ms(3);     // short wait time requirt
    lcdChangeLine(1);
}

void lcdChangeLine(uint n)
{
    switch (n)
    {
    case 1:
        lcdSendComand(0x80);
        break;
    case 2:
        lcdSendComand(0xC0);
        break;
    case 3:
        lcdSendComand(0x94);
        break;
    case 4:
        lcdSendComand(0xD4);
        break;
    }

    busy_wait_us(53);
}

void lcdClearLine(uint n)
{
    switch (n)
    {
    case 1:
        lcdSendComand(0x80);
        lcdSendString("                    ");
        lcdSendComand(0x80);
        break;
    case 2:
        lcdSendComand(0xC0);
        lcdSendString("                    ");
        lcdSendComand(0xC0);
        break;
    case 3:
        lcdSendComand(0x94);
        lcdSendString("                    ");
        lcdSendComand(0x94);
        break;
    case 4:
        lcdSendComand(0xD4);
        lcdSendString("                    ");
        lcdSendComand(0xD4);
        break;
    }
}