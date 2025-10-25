#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <math.h>
#include "led_strip.h"
#include "led_strip_types.h"
#include "led_strip_rmt.h"

#define BLINK_GPIO 48  // ← замени на свой GPIO (38 или 48)
#define NUM_LEDS 1

void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;
    float r_, g_, b_;

    if (h < 60)       { r_ = c; g_ = x; b_ = 0; }
    else if (h < 120) { r_ = x; g_ = c; b_ = 0; }
    else if (h < 180) { r_ = 0; g_ = c; b_ = x; }
    else if (h < 240) { r_ = 0; g_ = x; b_ = c; }
    else if (h < 300) { r_ = x; g_ = 0; b_ = c; }
    else              { r_ = c; g_ = 0; b_ = x; }

    *r = (uint8_t)((r_ + m) * 255);
    *g = (uint8_t)((g_ + m) * 255);
    *b = (uint8_t)((b_ + m) * 255);
}

extern "C" void app_main(void)
{
    /// LED strip common configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,  // The GPIO that connected to the LED strip's data line
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

    // Создаём драйвер
    led_strip_handle_t led_strip = NULL;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    // Очищаем
    led_strip_clear(led_strip);

    float hue = 0.0f;
    const float hue_step = 1.0f; // скорость смены цвета

    while (true) {
        uint8_t r, g, b;
        hsv_to_rgb(fmod(hue, 360.0f), 1.0f, 0.2f, &r, &g, &b);
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
        vTaskDelay(20 / portTICK_PERIOD_MS);
        // Выключить
        hue += hue_step;
    }
}
