#ifndef __SERVO_CONTROL_H__
#define __SERVO_CONTROL_H__

#include "project_config.h"

#if ENABLE_SERVO_CONTROL

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
#define SERVO_MIN_PULSE_WIDTH  1000  // 1ms for -90° (full left)
#define SERVO_CENTER_PULSE_WIDTH 1500 // 1.5ms for 0° (center)
#define SERVO_MAX_PULSE_WIDTH  2000  // 2ms for +90° (full right)
#define SERVO_PERIOD_US        20000 // 20ms period

// Input range constants
#define SERVO_INPUT_MIN        -100  // Minimum input value (full left)
#define SERVO_INPUT_MAX        100   // Maximum input value (full right)

// Function declarations
esp_err_t servo_control_init(void);
esp_err_t servo_control_set_position(int position);
void servo_control_deinit(void);

#endif // ENABLE_SERVO_CONTROL

#endif // __SERVO_CONTROL_H__