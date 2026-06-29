#include "project_config.h"

#if ENABLE_SERVO_CONTROL

#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <math.h>
#include "config.h"
#include "servo_control.h"

static const char *TAG = "servo_control";
static bool servo_initialized = false;
static servo_calibration_t servo_calibration = {
    .min_pulse_width = SERVO_DEFAULT_MIN_PULSE_WIDTH,
    .center_pulse_width = SERVO_DEFAULT_CENTER_PULSE_WIDTH,
    .max_pulse_width = SERVO_DEFAULT_MAX_PULSE_WIDTH,
};

static esp_err_t validate_calibration(const servo_calibration_t *calibration)
{
    if (calibration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (calibration->min_pulse_width < SERVO_ABSOLUTE_MIN_PULSE_WIDTH ||
        calibration->center_pulse_width < SERVO_ABSOLUTE_MIN_PULSE_WIDTH ||
        calibration->max_pulse_width < SERVO_ABSOLUTE_MIN_PULSE_WIDTH ||
        calibration->min_pulse_width > SERVO_ABSOLUTE_MAX_PULSE_WIDTH ||
        calibration->center_pulse_width > SERVO_ABSOLUTE_MAX_PULSE_WIDTH ||
        calibration->max_pulse_width > SERVO_ABSOLUTE_MAX_PULSE_WIDTH) {
        return ESP_ERR_INVALID_ARG;
    }

    if (calibration->min_pulse_width >= calibration->center_pulse_width ||
        calibration->center_pulse_width >= calibration->max_pulse_width) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static void load_calibration_from_config(void)
{
    config_data_t config_data = config_load();
    servo_calibration_t loaded_calibration = {
        .min_pulse_width = config_data.steering_min_pulse_width,
        .center_pulse_width = config_data.steering_center_pulse_width,
        .max_pulse_width = config_data.steering_max_pulse_width,
    };

    if (validate_calibration(&loaded_calibration) != ESP_OK) {
        ESP_LOGW(TAG, "Invalid persisted servo calibration; falling back to defaults");
        servo_calibration.min_pulse_width = SERVO_DEFAULT_MIN_PULSE_WIDTH;
        servo_calibration.center_pulse_width = SERVO_DEFAULT_CENTER_PULSE_WIDTH;
        servo_calibration.max_pulse_width = SERVO_DEFAULT_MAX_PULSE_WIDTH;
        return;
    }

    servo_calibration = loaded_calibration;
}

/**
 * @brief Calculate duty cycle for servo position
 * @param pulse_width_us Pulse width in microseconds
 * @return Duty cycle value for LEDC
 */
static uint32_t calculate_duty_cycle(uint32_t pulse_width_us)
{
    // Calculate duty cycle based on 13-bit resolution
    // Formula: duty = (pulse_width_us * (1 << SERVO_LEDC_DUTY_RES)) / SERVO_PERIOD_US
    uint32_t max_duty = (1 << SERVO_LEDC_DUTY_RES) - 1;
    uint32_t duty = (pulse_width_us * max_duty) / SERVO_PERIOD_US;
    return duty;
}

/**
 * @brief Convert position value (-100 to +100) to pulse width in microseconds
 * @param position Position value (-100 = full left, 0 = center, +100 = full right)
 * @return Pulse width in microseconds
 */
static uint32_t position_to_pulse_width(int position)
{
    // Clamp position to valid range
    if (position < SERVO_INPUT_MIN) position = SERVO_INPUT_MIN;
    if (position > SERVO_INPUT_MAX) position = SERVO_INPUT_MAX;

    if (position < 0) {
        return servo_calibration.center_pulse_width -
               ((-position * (servo_calibration.center_pulse_width - servo_calibration.min_pulse_width)) /
                (-SERVO_INPUT_MIN));
    }

    if (position > 0) {
        return servo_calibration.center_pulse_width +
               ((position * (servo_calibration.max_pulse_width - servo_calibration.center_pulse_width)) /
                SERVO_INPUT_MAX);
    }

    return servo_calibration.center_pulse_width;
}

esp_err_t servo_control_init(void)
{
    if (servo_initialized) {
        ESP_LOGW(TAG, "Servo control already initialized");
        return ESP_OK;
    }

    load_calibration_from_config();
    
    ESP_LOGI(TAG, "Initializing servo control on GPIO%d", SERVO_GPIO_PIN);
    
    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = SERVO_LEDC_DUTY_RES,
        .freq_hz = SERVO_LEDC_FREQUENCY,
        .speed_mode = SERVO_LEDC_MODE,
        .timer_num = SERVO_LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {
        .channel = SERVO_LEDC_CHANNEL,
        .duty = 0,
        .gpio_num = SERVO_GPIO_PIN,
        .speed_mode = SERVO_LEDC_MODE,
        .hpoint = 0,
        .timer_sel = SERVO_LEDC_TIMER,
        .flags.output_invert = 0
    };
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Mark as initialized before calling set_position
    servo_initialized = true;
    
    // Set servo to center position (0°)
    ret = servo_control_set_position(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial servo position: %s", esp_err_to_name(ret));
        servo_initialized = false;  // Reset on failure
        return ret;
    }
    
    ESP_LOGI(TAG, "Servo control initialized successfully - GPIO%d, Timer%d, Channel%d", 
             SERVO_GPIO_PIN, SERVO_LEDC_TIMER, SERVO_LEDC_CHANNEL);
    
    return ESP_OK;
}

esp_err_t servo_control_set_position(int position)
{
    if (!servo_initialized) {
        ESP_LOGE(TAG, "Servo control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Convert position to pulse width
    uint32_t pulse_width_us = position_to_pulse_width(position);
    
    // Calculate duty cycle
    uint32_t duty = calculate_duty_cycle(pulse_width_us);
    
    // Set duty cycle
    esp_err_t ret = ledc_set_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update duty cycle
    ret = ledc_update_duty(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Servo position set to %d (pulse width: %lu us, duty: %lu)", 
             position, pulse_width_us, duty);
    
    return ESP_OK;
}

esp_err_t servo_control_get_calibration(servo_calibration_t *calibration)
{
    if (calibration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *calibration = servo_calibration;
    return ESP_OK;
}

esp_err_t servo_control_apply_calibration(const servo_calibration_t *calibration, bool move_to_center)
{
    esp_err_t ret = validate_calibration(calibration);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Rejected invalid servo calibration min=%u center=%u max=%u",
                 calibration ? calibration->min_pulse_width : 0,
                 calibration ? calibration->center_pulse_width : 0,
                 calibration ? calibration->max_pulse_width : 0);
        return ret;
    }

    servo_calibration = *calibration;

    ESP_LOGI(TAG, "Servo calibration applied min=%u center=%u max=%u",
             servo_calibration.min_pulse_width,
             servo_calibration.center_pulse_width,
             servo_calibration.max_pulse_width);

    if (servo_initialized && move_to_center) {
        return servo_control_set_position(0);
    }

    return ESP_OK;
}

void servo_control_deinit(void)
{
    if (!servo_initialized) {
        ESP_LOGW(TAG, "Servo control not initialized");
        return;
    }
    
    // Stop LEDC channel
    ledc_stop(SERVO_LEDC_MODE, SERVO_LEDC_CHANNEL, 0);
    
    servo_initialized = false;
    ESP_LOGI(TAG, "Servo control deinitialized");
}

#endif // ENABLE_SERVO_CONTROL