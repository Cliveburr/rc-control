#ifndef __SERVO_CONTROL_H__
#define __SERVO_CONTROL_H__

#include "project_config.h"

#if ENABLE_SERVO_CONTROL

#include <driver/ledc.h>
#include <esp_err.h>
#include <inttypes.h>
#include <stdbool.h>

// GPIO pin for servo control signal
#define SERVO_GPIO_PIN    4   // GPIO4 for servo control signal

// Servo PWM configuration
#define SERVO_LEDC_TIMER       LEDC_TIMER_1
#define SERVO_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CHANNEL     LEDC_CHANNEL_1
#define SERVO_LEDC_DUTY_RES    LEDC_TIMER_13_BIT  // 13-bit resolution for precise control
#define SERVO_LEDC_FREQUENCY   50                 // 50Hz for standard servo

// Servo timing constants (in microseconds)
#define SERVO_DEFAULT_MIN_PULSE_WIDTH  1000  // Typical minimum pulse; mechanical angle depends on the servo
#define SERVO_DEFAULT_CENTER_PULSE_WIDTH 1500 // Typical center pulse; adjust for steering trim if needed
#define SERVO_DEFAULT_MAX_PULSE_WIDTH  2000  // Typical maximum pulse; increase carefully only after validation
#define SERVO_ABSOLUTE_MIN_PULSE_WIDTH 500
#define SERVO_ABSOLUTE_MAX_PULSE_WIDTH 2500
#define SERVO_PERIOD_US        20000 // 20ms period

// Input range constants
#define SERVO_INPUT_MIN        -100  // Minimum input value (full left)
#define SERVO_INPUT_MAX        100   // Maximum input value (full right)

typedef struct {
	uint16_t min_pulse_width;
	uint16_t center_pulse_width;
	uint16_t max_pulse_width;
} servo_calibration_t;

// Function declarations
esp_err_t servo_control_init(void);
esp_err_t servo_control_set_position(int position);
esp_err_t servo_control_get_calibration(servo_calibration_t *calibration);
esp_err_t servo_control_apply_calibration(const servo_calibration_t *calibration, bool move_to_center);
void servo_control_deinit(void);

#endif // ENABLE_SERVO_CONTROL

#endif // __SERVO_CONTROL_H__