#include <esp_log.h>
#include <driver/gpio.h>
#include "led_control.h"

static const char *TAG = "led_control";

static bool light_state = false;
static bool horn_state = false;

void led_control_init(void)
{
    ESP_LOGI(TAG, "Initializing LED control");
    
    // Configure GPIO pins for LED output (sinking mode - active LOW)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_LIGHT_PIN) | (1ULL << LED_HORN_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO pins: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize LEDs to OFF state (HIGH for sinking mode)
    gpio_set_level(LED_LIGHT_PIN, 1);
    gpio_set_level(LED_HORN_PIN, 1);
    
    light_state = false;
    horn_state = false;
    
    ESP_LOGI(TAG, "LED control initialized - Light: GPIO%d, Horn: GPIO%d", LED_LIGHT_PIN, LED_HORN_PIN);
}

void led_light_set(bool state)
{
    light_state = state;
    // For sinking mode: LOW = LED ON, HIGH = LED OFF
    gpio_set_level(LED_LIGHT_PIN, !state);
    ESP_LOGI(TAG, "Light LED %s", state ? "ON" : "OFF");
}

void led_horn_set(bool state)
{
    horn_state = state;
    // For sinking mode: LOW = LED ON, HIGH = LED OFF
    gpio_set_level(LED_HORN_PIN, !state);
    ESP_LOGI(TAG, "Horn LED %s", state ? "ON" : "OFF");
}

void led_light_toggle(void)
{
    led_light_set(!light_state);
}

void led_horn_toggle(void)
{
    led_horn_set(!horn_state);
}
