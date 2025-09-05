#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hx711.pio.h"
#include "encoder.pio.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/ticks.h"
#include "hardware/timer.h"
#include "math.h"
#include "pico/multicore.h"

#include "lcd.h"

// gpio Pins
#define HX711_DOUT 5
#define HX711_PD_SCK 6

#define Encoder_A 12
#define Encoder_B 13
#define Encoder_Button 11

#define GPIO_BUTTON 7
#define GPIO_RELAY 3

// for pio
#define PIO_SM_Clock 200000 // 200 kHz clock

#define PIO_HX711 pio0
#define PIO_ENCODER pio1

#define GAIN 1802

uint sm_encoder = 0;
uint sm_hx711 = 0;

// globel Varibals
uint32_t value = 0;
float whight = 0;
float tarra = 230772.8851;
float lastStopp = 0;
bool startgrind = false;
float stopWhight = 18.0;
uint8_t selectetRow = 1;
bool entryMode = false;

void ui_handling();

// interupt handler, pio encoder

void pio_irq_handler()
{
    if (entryMode)
    {
        switch (selectetRow)
        {
        case 1:
            if (PIO_ENCODER->irq & 1)
            {
                stopWhight += 0.1;
            }

            if (PIO_ENCODER->irq & 2)
            {
                stopWhight -= 0.1;
            }
        }
    }
    else
    {

        if (PIO_ENCODER->irq & 1)
        {
            selectetRow++;
        }

        if (PIO_ENCODER->irq & 2)
        {
            selectetRow--;
        }

        if (selectetRow < 1)
        {
            selectetRow = 4;
        }
        else if (selectetRow > 4)
        {
            selectetRow = 1;
        }
    }

    PIO_ENCODER->irq = 3;
}

// tara the whight
void taraf()
{
    int sizeTarra = 200;
    long sum = 0;

    for (int i = 0; i < sizeTarra; i++)
    {
        sum += pio_sm_get(PIO_HX711, sm_hx711);
        busy_wait_ms(10);
    }

    tarra = (float)sum / sizeTarra;
}

// interupt handler for gpio irq (putton and encoder_button)
void gpio_irq_handler(uint gpio, uint32_t event_mask)
{
    if (gpio == GPIO_BUTTON)
    {
        if (!startgrind)
        {
            // tara wight
            taraf();

            startgrind = true;
        }
    }

    if (gpio == Encoder_Button)
    {
        switch (selectetRow)
        {
        case 1:
            entryMode = !entryMode;
            break;

        case 2:
            break;

        case 3:
            break;

        case 4:
            lcdChangeLine(4);
            lcdSendData(0xff);
            taraf();
            break;
        }
    }
}

int main()

{
    { // init block
        stdio_init_all();

        // I2C Initialisation. Using it at 100Khz.
        i2c_init(i2c_default, 100 * 1000);
        gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
        gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
        gpio_pull_up(I2C_SDA);
        gpio_pull_up(I2C_SCL);

        // startup lcd
        lcdInit();
        lcdSendString("Starting...");

        // gpio init
        gpio_init(GPIO_BUTTON);
        gpio_init(GPIO_RELAY);
        gpio_init(Encoder_Button);

        gpio_set_dir(GPIO_BUTTON, GPIO_IN);
        gpio_set_dir(GPIO_RELAY, GPIO_OUT);
        gpio_set_dir(Encoder_Button, GPIO_IN);
        gpio_set_irq_enabled_with_callback(Encoder_Button, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
        gpio_set_irq_enabled_with_callback(GPIO_BUTTON, GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler);

        // sets up the pio sm for hx711
        //******************************************************
        uint sm_hx711 = pio_claim_unused_sm(PIO_HX711, true);                           // clams free pio sm
        uint offset = pio_add_program(PIO_HX711, &hx711_program);                       // add program to instruction memory
        float div = (float)clock_get_hz(clk_sys) / PIO_SM_Clock;                        // calc clock div for sm_clock 200 khz
        hx711_program_init(PIO_HX711, sm_hx711, offset, HX711_PD_SCK, HX711_DOUT, div); // start running pio
        pio_sm_set_enabled(PIO_HX711, sm_hx711, true);
        //**********************************************************

        // sets up the pio sm for Encoder
        //******************************************************
        uint sm_encoder = pio_claim_unused_sm(PIO_ENCODER, true);                    // clams free pio sm
        offset = pio_add_program(PIO_ENCODER, &encoder_program);                     // add program to instruction memory
        encoder_program_init(PIO_ENCODER, sm_encoder, offset, Encoder_A, Encoder_B); // start running pio
        pio_sm_set_enabled(PIO_ENCODER, sm_encoder, true);
        irq_set_exclusive_handler(PIO1_IRQ_0, pio_irq_handler);
        irq_set_enabled(PIO1_IRQ_0, true);
        //**********************************************************

        busy_wait_ms(2000);

        taraf();
        // LCD and encoder are controlt by the secend core
        // grinding and wight is controled by first core
        multicore_launch_core1(ui_handling);

        // printf("Inite done\n\r");
    }

    // Variabl init for core 0
    int sizeMean = 20;   // size of floating Mean array
    int array[sizeMean]; // array for floating mean
    int count = 0;

    // main program loop
    while (true)
    {
        int sum = 0;
        array[count] = pio_sm_get(PIO_HX711, sm_hx711);
        count++;

        for (int j = 0; j < sizeMean; j++)
        {
            sum += array[j];
        }

        value = (sum / sizeMean);
        whight = (value - tarra) / GAIN;

        if (count >= sizeMean)
        {
            count = 0;
        }

        busy_wait_ms(5);

        if (startgrind)
        {
            gpio_put(GPIO_RELAY, true);

            if (whight > stopWhight)
            {
                gpio_put(GPIO_RELAY, false);
                startgrind = false;
                lastStopp = whight;
            }
        }
    }
    return 0;
}

// program for Core 1 Handls the LCD and the encoder
void ui_handling()
{

    char string[20]; // string Buffer for sprintf

    while (true)
    {
        // writes Soll value to lcd
        sprintf(string, " Soll:   %.1f g", stopWhight);
        lcdClearLine(1);
        lcdSendString(string);

        // writes is value to lcd
        sprintf(string, " Ist:    %.1f g", whight);
        lcdClearLine(2);
        lcdSendString(string);

        // writes last value to lcd
        lcdClearLine(3);
        sprintf(string, " letzte: %.1f g", lastStopp);
        lcdSendString(string);

        lcdClearLine(4);
        lcdSendString(" Tara");

        if (startgrind)
        {
            lcdSendString("     mahle...");
        }

        lcdChangeLine(selectetRow);

        if (entryMode)
        {
            lcdSendData(0xff);
        }
        else
        {
            lcdSendString(">");
        }

        busy_wait_ms(100);
    }
}