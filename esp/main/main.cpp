//================================================================================================//

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <portmacro.h>

//------------------------------------------------------------------------------------------------//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include "led_strip_rmt.h"

//================================================================================================//

static const size_t kRxBufferSize = 1024;

void uart_init(void);
void receive_task(void *arg);

//================================================================================================//

extern "C" void
app_main(void)
{
    // Strip common config info
    led_strip_config_t strip_config =
    {
        .strip_gpio_num         = 48,
        .max_leds               = 1,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags                  =
        {
            .invert_out = false,
        }
    };

    // RMT backend specific config info
    led_strip_rmt_config_t rmt_config =
    {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags             =
        {
            .with_dma = false,
        }
    };

    // Creating led strip handler
    led_strip_handle_t led_strip = NULL;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    // Initializing UART
    uart_init();

    // Creating task to receive data from UART
    xTaskCreate(receive_task, "uart_rx_task", 2048, &led_strip, 5, NULL);
}

//================================================================================================//

void
uart_init(void)
{
    const uart_config_t uart_config =
    {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, kRxBufferSize * 2, 0, 0, NULL, 0);
}

//------------------------------------------------------------------------------------------------//

void
receive_task(void *arg)
{
    led_strip_handle_t led_strip = *(led_strip_handle_t *)arg;

    struct res_t
    {
        char     symbol;
        uint32_t red;
        uint32_t green;
        uint32_t blue;
    };

    res_t res[] =
    {
        {'r', 255,   0,   0},
        {'g',   0, 255,   0},
        {'b',   0,   0, 255},
    };

    uint8_t data[kRxBufferSize] = {};
    while (true)
    {
        // FIXME Blinking LED
        led_strip_set_pixel(led_strip, 0, 5, 5, 5);
        led_strip_refresh(led_strip);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
        led_strip_refresh(led_strip);
        vTaskDelay(50 / portTICK_PERIOD_MS);

        // Reading bytes from UART
        int len = uart_read_bytes(UART_NUM_0, data, kRxBufferSize, 20 / portTICK_PERIOD_MS);
        //
        // Handling received data
        // Drawing colors if got ascii 'r', 'g' or 'b'
        if (len > 0)
        {
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
}

//================================================================================================//
