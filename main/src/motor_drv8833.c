#include "project_config.h"

#if ENABLE_MOTOR_CONTROL && (MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_DRV8833)

#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <string.h>
#include "motor_drv8833.h"

static const char *TAG = "drv8833";
static bool drv8833_initialized = false;
static motor_state_t drv8833_state = {
    .speed = 0,
    .mode = MOTOR_MODE_FREE,
    .enabled = false
};

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

/**
 * @brief Configure GPIO pins for DRV8833
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t configure_gpio(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << DRV8833_IN1_PIN) | (1ULL << DRV8833_IN2_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set initial state to free mode
    gpio_set_level(DRV8833_IN1_PIN, DRV8833_FREE_MODE_LOW ? 0 : 1);
    gpio_set_level(DRV8833_IN2_PIN, DRV8833_FREE_MODE_LOW ? 0 : 1);

    ESP_LOGI(TAG, "GPIO pins configured - IN1: GPIO%d, IN2: GPIO%d", 
             DRV8833_IN1_PIN, DRV8833_IN2_PIN);
    return ESP_OK;
}

/**
 * @brief Configure LEDC timers and channels for PWM
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t configure_ledc(void)
{
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = DRV8833_LEDC_DUTY_RES,
        .freq_hz = DRV8833_LEDC_FREQUENCY,
        .speed_mode = DRV8833_LEDC_MODE,
        .timer_num = DRV8833_LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel for IN1
    ledc_channel_config_t ledc_channel_in1 = {
        .channel = DRV8833_LEDC_IN1_CHANNEL,
        .duty = 0,
        .gpio_num = DRV8833_IN1_PIN,
        .speed_mode = DRV8833_LEDC_MODE,
        .hpoint = 0,
        .timer_sel = DRV8833_LEDC_TIMER,
        .flags.output_invert = 0
    };

    ret = ledc_channel_config(&ledc_channel_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel IN1: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure LEDC channel for IN2
    ledc_channel_config_t ledc_channel_in2 = {
        .channel = DRV8833_LEDC_IN2_CHANNEL,
        .duty = 0,
        .gpio_num = DRV8833_IN2_PIN,
        .speed_mode = DRV8833_LEDC_MODE,
        .hpoint = 0,
        .timer_sel = DRV8833_LEDC_TIMER,
        .flags.output_invert = 0
    };

    ret = ledc_channel_config(&ledc_channel_in2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel IN2: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "LEDC configured - Timer: %d, Frequency: %dHz, Resolution: %d-bit",
             DRV8833_LEDC_TIMER, DRV8833_LEDC_FREQUENCY, (1 << DRV8833_LEDC_DUTY_RES));

    return ESP_OK;
}

/**
 * @brief Set PWM duty cycle for a channel
 * @param channel LEDC channel
 * @param duty_percent Duty cycle percentage (0-100)
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t set_pwm_duty(ledc_channel_t channel, int duty_percent)
{
    if (duty_percent < 0) duty_percent = 0;
    if (duty_percent > 100) duty_percent = 100;

    uint32_t duty = (duty_percent * DRV8833_MAX_DUTY) / 100;
    
    esp_err_t ret = ledc_set_duty(DRV8833_LEDC_MODE, channel, duty);
    if (ret != ESP_OK) {
        return ret;
    }

    return ledc_update_duty(DRV8833_LEDC_MODE, channel);
}

/**
 * @brief Apply motor control signals to DRV8833
 * @param mode Motor mode
 * @param speed_percent Speed percentage (0-100) for PWM modes
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t apply_motor_control(motor_mode_t mode, int speed_percent)
{
    esp_err_t ret = ESP_OK;

    switch (mode) {
        case MOTOR_MODE_FORWARD:
            // Forward: IN1=PWM, IN2=LOW
            ret = set_pwm_duty(DRV8833_LEDC_IN1_CHANNEL, speed_percent);
            if (ret == ESP_OK) {
                ret = set_pwm_duty(DRV8833_LEDC_IN2_CHANNEL, 0);
            }
            ESP_LOGD(TAG, "Forward mode: IN1=%d%%, IN2=0%%", speed_percent);
            break;

        case MOTOR_MODE_REVERSE:
            // Reverse: IN1=LOW, IN2=PWM
            ret = set_pwm_duty(DRV8833_LEDC_IN1_CHANNEL, 0);
            if (ret == ESP_OK) {
                ret = set_pwm_duty(DRV8833_LEDC_IN2_CHANNEL, speed_percent);
            }
            ESP_LOGD(TAG, "Reverse mode: IN1=0%%, IN2=%d%%", speed_percent);
            break;

        case MOTOR_MODE_BRAKE:
            // Brake mode: Based on configuration
            if (DRV8833_BRAKE_MODE_HIGH) {
                // IN1=HIGH, IN2=HIGH
                ret = set_pwm_duty(DRV8833_LEDC_IN1_CHANNEL, 100);
                if (ret == ESP_OK) {
                    ret = set_pwm_duty(DRV8833_LEDC_IN2_CHANNEL, 100);
                }
                ESP_LOGD(TAG, "Brake mode: IN1=HIGH, IN2=HIGH");
            } else {
                // IN1=LOW, IN2=LOW
                ret = set_pwm_duty(DRV8833_LEDC_IN1_CHANNEL, 0);
                if (ret == ESP_OK) {
                    ret = set_pwm_duty(DRV8833_LEDC_IN2_CHANNEL, 0);
                }
                ESP_LOGD(TAG, "Brake mode: IN1=LOW, IN2=LOW");
            }
            break;

        case MOTOR_MODE_FREE:
            // Free mode: Based on configuration
            if (DRV8833_FREE_MODE_LOW) {
                // IN1=LOW, IN2=LOW
                ret = set_pwm_duty(DRV8833_LEDC_IN1_CHANNEL, 0);
                if (ret == ESP_OK) {
                    ret = set_pwm_duty(DRV8833_LEDC_IN2_CHANNEL, 0);
                }
                ESP_LOGD(TAG, "Free mode: IN1=LOW, IN2=LOW");
            } else {
                // IN1=HIGH, IN2=HIGH
                ret = set_pwm_duty(DRV8833_LEDC_IN1_CHANNEL, 100);
                if (ret == ESP_OK) {
                    ret = set_pwm_duty(DRV8833_LEDC_IN2_CHANNEL, 100);
                }
                ESP_LOGD(TAG, "Free mode: IN1=HIGH, IN2=HIGH");
            }
            break;

        default:
            ESP_LOGE(TAG, "Unknown motor mode: %d", mode);
            return ESP_ERR_INVALID_ARG;
    }

    return ret;
}

// =============================================================================
// DRIVER INTERFACE IMPLEMENTATION
// =============================================================================

esp_err_t drv8833_init(void)
{
    if (drv8833_initialized) {
        ESP_LOGW(TAG, "DRV8833 already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing DRV8833 motor driver");

    // Configure GPIO pins
    esp_err_t ret = configure_gpio();
    if (ret != ESP_OK) {
        return ret;
    }

    // Configure LEDC for PWM
    ret = configure_ledc();
    if (ret != ESP_OK) {
        return ret;
    }

    // Set initial state
    drv8833_state.speed = 0;
    drv8833_state.mode = MOTOR_MODE_FREE;
    drv8833_state.enabled = true;

    // Apply initial state (free mode)
    ret = apply_motor_control(MOTOR_MODE_FREE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial motor state");
        return ret;
    }

    drv8833_initialized = true;
    ESP_LOGI(TAG, "DRV8833 motor driver initialized successfully");

    return ESP_OK;
}

esp_err_t drv8833_deinit(void)
{
    if (!drv8833_initialized) {
        ESP_LOGW(TAG, "DRV8833 not initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing DRV8833 motor driver");

    // Stop motor before deinit
    apply_motor_control(MOTOR_MODE_FREE, 0);

    // Reset GPIO pins to input mode
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << DRV8833_IN1_PIN) | (1ULL << DRV8833_IN2_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);

    drv8833_state.enabled = false;
    drv8833_initialized = false;

    ESP_LOGI(TAG, "DRV8833 motor driver deinitialized");
    return ESP_OK;
}

esp_err_t drv8833_set_speed(int speed)
{
    if (!drv8833_initialized) {
        ESP_LOGE(TAG, "DRV8833 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Clamp speed to valid range
    if (speed < -100) speed = -100;
    if (speed > 100) speed = 100;

    ESP_LOGI(TAG, "Setting DRV8833 speed: %d", speed);

    motor_mode_t mode;
    int abs_speed;

    if (speed > 0) {
        mode = MOTOR_MODE_FORWARD;
        abs_speed = speed;
    } else if (speed < 0) {
        mode = MOTOR_MODE_REVERSE;
        abs_speed = -speed;
    } else {
        mode = MOTOR_MODE_BRAKE;
        abs_speed = 0;
    }

    esp_err_t ret = apply_motor_control(mode, abs_speed);
    if (ret == ESP_OK) {
        drv8833_state.speed = speed;
        drv8833_state.mode = mode;
    }

    return ret;
}

esp_err_t drv8833_set_mode(motor_mode_t mode)
{
    if (!drv8833_initialized) {
        ESP_LOGE(TAG, "DRV8833 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Setting DRV8833 mode: %d", mode);

    esp_err_t ret = apply_motor_control(mode, 0);
    if (ret == ESP_OK) {
        drv8833_state.mode = mode;
        if (mode == MOTOR_MODE_BRAKE || mode == MOTOR_MODE_FREE) {
            drv8833_state.speed = 0;
        }
    }

    return ret;
}

esp_err_t drv8833_stop(void)
{
    if (!drv8833_initialized) {
        ESP_LOGE(TAG, "DRV8833 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping DRV8833 motor");

    esp_err_t ret = apply_motor_control(MOTOR_MODE_BRAKE, 0);
    if (ret == ESP_OK) {
        drv8833_state.speed = 0;
        drv8833_state.mode = MOTOR_MODE_BRAKE;
    }

    return ret;
}

esp_err_t drv8833_get_state(motor_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!drv8833_initialized) {
        ESP_LOGE(TAG, "DRV8833 not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(state, &drv8833_state, sizeof(motor_state_t));
    return ESP_OK;
}

// =============================================================================
// DRIVER INTERFACE REGISTRATION
// =============================================================================

static const motor_driver_interface_t drv8833_interface = {
    .name = "DRV8833",
    .init = drv8833_init,
    .deinit = drv8833_deinit,
    .set_speed = drv8833_set_speed,
    .set_mode = drv8833_set_mode,
    .stop = drv8833_stop,
    .get_state = drv8833_get_state
};

const motor_driver_interface_t* drv8833_get_interface(void)
{
    return &drv8833_interface;
}

#endif // ENABLE_MOTOR_CONTROL && (MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_DRV8833)