#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include "led_strip_rmt.h"

#define RX_BUF_SIZE 1024

void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

void receive_task(void *arg)
{
    led_strip_handle_t led_strip = *(led_strip_handle_t *)arg;

    struct res_t
    {
        char symbol;
        uint32_t red;
        uint32_t green;
        uint32_t blue;
    };

    res_t res[] =
    {
        {'r', 255, 0, 0},
        {'g', 0, 255, 0},
        {'b', 0, 0, 255},
    };

    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE);
    while (1) {
        led_strip_set_pixel(led_strip, 0, 5, 5, 5);
        led_strip_refresh(led_strip);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
        led_strip_refresh(led_strip);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        int len = uart_read_bytes(UART_NUM_0, data, RX_BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            for (int i = 0; i < len; ++i)
            {
                for (int j = 0; j < sizeof(res) / sizeof(res[0]); ++j)
                {
                    if (data[i] == res[j].symbol)
                    {
                        led_strip_set_pixel(led_strip, 0, res[j].red, res[j].green, res[j].blue);
                        led_strip_refresh(led_strip);
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                    }
                }
            }
        }
    }
    free(data);
}

extern "C" void app_main(void)
{
    /// LED strip common configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = 48,  // The GPIO that connected to the LED strip's data line
        .max_leds = 1,                 // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812, // LED strip model, it determines the bit timing
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };

    /// RMT backend specific configuration
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
        .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
        .flags = {
            .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
        }
    };

    led_strip_handle_t led_strip = NULL;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // Очищаем
    led_strip_clear(led_strip);

    uart_init();
    printf("Waiting for data on UART0 (USB-CDC)...\n");
    xTaskCreate(receive_task, "uart_rx_task", 2048, &led_strip, 5, NULL);
}
