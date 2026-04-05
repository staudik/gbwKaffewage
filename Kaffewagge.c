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
#include "kaffeeBean.h"

// gpio Pins
#define HX711_DOUT 5
#define HX711_PD_SCK 6

#define Encoder_A 11
#define Encoder_B 12
#define Encoder_Button 13

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

// for displays
#define Char_High 8
#define Char_width 6

#define SizeMean 3 // size of floating Mean array

uint sm_encoder = 0;
uint sm_hx711 = 0;

// init sturct for display
ssd1306_t disp;

// globel Varibals
uint32_t value = 0;        // row value for wight
float whight = 0;          // current calc. whight
float tarra = 230772.8851; // current tarra value
float lastStopp = 0;       // last whight that was ground
float offset = -1.0;       // offset for stopp
bool startgrind = false;
float stopWhight = 18.0; // whight to stop at
bool entryMode = false;  // is cursure in entrymode

typedef enum
{
    mainMenu,
    Settings,
    grinding
} displayState;
displayState dispState = mainMenu;

typedef enum
{
    soll,
    tara,
    grind,
    settingsButton,
    offsetSetting,
    backToManu,
} selectetElement;
selectetElement selectEle = soll;

void ui_handling();

// interupt handler, pio encoder
void pio_irq_handler()
{
    if (entryMode)
    {
        switch (dispState)
        {
        case mainMenu:
            switch (selectEle)
            {
            case soll:
                if (PIO_ENCODER->irq & 1)
                {
                    stopWhight += 0.1;
                }

                if (PIO_ENCODER->irq & 2)
                {
                    stopWhight -= 0.1;
                }
            }
            break;
        case Settings:
            switch (selectEle)
            {
            case offsetSetting:
                if (PIO_ENCODER->irq & 1)
                {
                    offset += 0.05;
                }

                if (PIO_ENCODER->irq & 2)
                {
                    offset -= 0.05;
                }
            }
            break;
        }
    }
    else
    {

        if (PIO_ENCODER->irq & 1)
        {
            selectEle++;
        }
        if (PIO_ENCODER->irq & 2)
        {
            selectEle--;
        }

        switch (dispState)
        {
        case mainMenu:
            if (selectEle < soll)
            {
                selectEle = settingsButton;
            }
            else if (selectEle > settingsButton)
            {
                selectEle = soll;
            }
            break;
        case Settings:
            if (selectEle < offsetSetting)
            {
                selectEle = backToManu;
            }
            else if (selectEle > backToManu)
            {
                selectEle = offsetSetting;
            }
            break;
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

// interupt handler for gpio irq encoder_button
void gpio_irq_handler(uint gpio, uint32_t event_mask)
{
    if (gpio == Encoder_Button)
    {
        if (event_mask & GPIO_IRQ_EDGE_FALL)
        {
            switch (selectEle)
            {
            case soll:
                entryMode = !entryMode;
                break;
            case offsetSetting:
                entryMode = !entryMode;
                break;
            case tara:
                taraf();
                break;
            case settingsButton:
                dispState = Settings;
                break;
            case backToManu:
                dispState = mainMenu;
                break;
            case grind:
                startgrind = true;
                break;
            }
        }
    }
}

// updates wight reading
void updateWight()
{
    static int array[SizeMean]; // array for rolling mean
    static int count = 0;
    int sum = 0;

    // reading value form pio
    array[count] = pio_sm_get(PIO_HX711, sm_hx711);
    count++;

    // calc. sum for rolling mean
    for (int j = 0; j < SizeMean; j++)
    {
        sum += array[j];
    }

    value = (sum / SizeMean);
    whight = (value - tarra) / GAIN;

    if (count >= SizeMean)
    {
        count = 0;
    }
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

        // gpio init
        gpio_init(GPIO_RELAY);
        gpio_init(Encoder_Button);

        gpio_set_dir(GPIO_RELAY, GPIO_OUT);
        gpio_set_dir(Encoder_Button, GPIO_IN);
        gpio_disable_pulls(Encoder_Button);
        gpio_set_input_hysteresis_enabled(Encoder_Button, true);
        gpio_set_irq_enabled_with_callback(Encoder_Button, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

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

        taraf();

        // LCD and encoder are controlt by the secend core
        // grinding and wight is controled by first core
        multicore_launch_core1(ui_handling);

        // printf("Inite done\n\r");
    }

    // main program loop
    while (true)
    {
        if (startgrind)
        {
            dispState = grinding;
            taraf();
            gpio_put(GPIO_RELAY, true);
            startgrind = false;
        }

        updateWight();

        if ((whight > stopWhight + offset) && gpio_get_out_level(GPIO_RELAY))
        {
            gpio_put(GPIO_RELAY, false);
            busy_wait_ms(100);
            updateWight();
            dispState = mainMenu;
            lastStopp = whight;
        }

        printf("%f\n\r", whight);
        busy_wait_ms(80);
    }
    return 0;
}

void ui_draw_grindingScreen()
{
    ssd1306_clear(&disp);
    char string[64];

    sprintf(string, "mahle...");
    ssd1306_draw_string(&disp, 3, 3, 2, string);

    sprintf(string, "ist:%.1fg", whight);
    ssd1306_draw_string(&disp, 3, 3 + Char_High * 2 + 2, 2, string);

    ssd1306_show(&disp);
}

// draws settings menu
void ui_draw_settings()
{
    ssd1306_clear(&disp);
    char string[64];

    sprintf(string, "Offset:");
    ssd1306_draw_string(&disp, 3, 3, 2, string);

    sprintf(string, "%.2f", offset);
    ssd1306_draw_string(&disp, 3, 4 + Char_High * 2, 2, string);

    sprintf(string, "main Menu");
    ssd1306_draw_string(&disp, 10, 54, 1, string);

    switch (selectEle)
    {
    case offsetSetting:
        ssd1306_draw_empty_square(&disp, 1, 1, 6 * Char_width * 2 + 2, Char_High * 2 + 2);
        break;
    case backToManu:
        ssd1306_draw_empty_square(&disp, 8, 52, 9 * Char_width + 2, Char_High + 2);
        break;
    }

    ssd1306_show(&disp);
}

// draws the main menu with current data
void ui_draw_main_menu()
{
    ssd1306_clear(&disp);
    char string[64]; // string Buffer for sprintf

    sprintf(string, "Soll:%.1fg", stopWhight);
    ssd1306_draw_string(&disp, 3, 3, 2, string);

    sprintf(string, "last:%.1fg", lastStopp);
    ssd1306_draw_string(&disp, 3, 3 + Char_High * 2 + 2, 2, string);

    sprintf(string, "Tara");
    ssd1306_draw_string(&disp, 3, 3 + Char_High * 4 + 2, 1, string);

    sprintf(string, "Settings");
    ssd1306_draw_string(&disp, 9, 54, 1, string);

    ssd1306_bmp_show_image_with_offset(&disp, monokaffe_bmp_data, monokaffe_bmp_size, 70, 40);

    switch (selectEle)
    {
    case soll:
        ssd1306_draw_empty_square(&disp, 1, 1, (5 * Char_width) * 2 + 2, Char_High * 2 + 2);
        break;
    case settingsButton:
        ssd1306_draw_empty_square(&disp, 7, 52, 8 * Char_width + 2, Char_High + 2);
        break;
    case tara:
        ssd1306_draw_empty_square(&disp, 1, 3 + Char_High * 4, 4 * Char_width + 2, Char_High + 2);
        break;
    case grind:
        ssd1306_draw_empty_square(&disp, 70, 40, 19, 19);
        break;
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
        case grinding:
            ui_draw_grindingScreen();
            break;
        }

        // printf("selectElement: %i\n", selectEle);
        // printf("entryMode %i\n", entryMode);
        busy_wait_ms(100);
    }
}
