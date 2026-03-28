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
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "ssd1306.h"

// gpio Pins
#define HX711_DOUT 5
#define HX711_PD_SCK 6

#define Encoder_A 12
#define Encoder_B 13
#define Encoder_Button 11

#define GPIO_BUTTON 7
#define GPIO_RELAY 3

#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

#define SSD1306_ADDRESS 0x3c

// for pio
#define PIO_SM_Clock 200000 // 200 kHz clock

#define PIO_HX711 pio0
#define PIO_ENCODER pio1

#define GAIN 1802

uint sm_encoder = 0;
uint sm_hx711 = 0;

// init sturct for display
ssd1306_t disp;

// globel Varibals
uint32_t value = 0;
float whight = 0;          // current calc. whight
float tarra = 230772.8851; // current tarra value
float lastStopp = 0;       // last whight that was ground
bool startgrind = false;
float stopWhight = 18.0;     // whight to stop at
uint8_t selectetElement = 1; // Row selectet with cursure
bool entryMode = false;      // is cursure in entrymode

typedef enum
{
    mainMenu,
    Settings
} displayState;
displayState dispState = mainMenu;

typedef enum
{
    Soll,
    settingsButton,
    offset
} selectetElement;
selectetElement selectetElement = Soll;

void ui_handling();

// interupt handler, pio encoder
void pio_irq_handler()
{
    if (entryMode)
    {
        switch (selectetElement)
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
            selectetElement++;
        }

        if (PIO_ENCODER->irq & 2)
        {
            selectetElement--;
        }

        if (selectetElement < 1)
        {
            selectetElement = 4;
        }
        else if (selectetElement > 4)
        {
            selectetElement = 1;
        }
    }

    PIO_ENCODER->irq = 3;
}

// tara the whight
void taraf()
{
    int sizeTarra = 50;
    long sum = 0;

    for (int i = 0; i < sizeTarra; i++)
    {
        sum += pio_sm_get(PIO_HX711, sm_hx711);
        busy_wait_ms(90);
    }

    tarra = (float)sum / sizeTarra;
}

// interupt handler for gpio irq (putton and encoder_button)
void gpio_irq_handler(uint gpio, uint32_t event_mask)
{
}

int main()

{
    { // init block
        stdio_init_all();

        // I2C Initialisation. Using it at 400Khz.
        i2c_init(i2c_default, 400 * 1000);
        gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
        gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
        gpio_pull_up(I2C_SDA);
        gpio_pull_up(I2C_SCL);

        // OLED init

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
    int sizeMean = 3;    // size of floating Mean array
    int array[sizeMean]; // array for floating mean
    int count = 0;
    int sum = 0;

    // main program loop
    while (true)
    {
        sum = 0;
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

        if (startgrind)
        {
            gpio_put(GPIO_RELAY, true);

            if (whight > stopWhight)
            {
                gpio_put(GPIO_RELAY, false);
                startgrind = false;
                busy_wait_ms(10);
                lastStopp = whight;
            }
        }
        printf("%f\n\r", whight);
        busy_wait_ms(80);
    }
    return 0;
}

// draws settings menu
void ui_draw_settings()
{
    ssd1306_clear(&disp);
    char string[64];

    sprintf(string, "main Menu");
    ssd1306_draw_string(&disp, 10, 54, 1, string);

    ssd1306_show(&disp);
}

// draws the main menu with current data
void ui_draw_main_menu()
{
    ssd1306_clear(&disp);
    char string[64]; // string Buffer for sprintf

    sprintf(string, "Soll:%.1fg", stopWhight);
    ssd1306_draw_string(&disp, 2, 2, 2, string);

    sprintf(string, "Ist:%.1fg", whight);
    ssd1306_draw_string(&disp, 0, 20, 2, string);

    sprintf(string, "letzte: %.1f g", lastStopp);
    ssd1306_draw_string(&disp, 0, 40, 1, string);

    sprintf(string, "Settings");
    ssd1306_draw_string(&disp, 9, 54, 1, string);

    switch (selectetElement)
    {
    case Soll:
        ssd1306_draw_empty_square(&disp, 1, 1, 4 * 6 + 1, 20);
        break;
    case settingsButton:
        ssd1306_draw_empty_square(&disp, 8, 53, 8 * 6 + 1, 10);
    }

    ssd1306_show(&disp);
}

// program for Core 1 Handels the Display and the encoder
void ui_handling()
{

    char string[64]; // string Buffer for sprintf

    disp.external_vcc = false;

    // init Display and clear
    ssd1306_init(&disp, 128, 64, SSD1306_ADDRESS, I2C_PORT);
    ssd1306_clear(&disp);
    ssd1306_show(&disp);

    while (true)
    {
        switch (dispState)
        {
        case mainMenu:
            ui_draw_main_menu();
            break;
        case Settings:
            ui_draw_settings();
            break;
        }
        busy_wait_ms(250);
    }
}
