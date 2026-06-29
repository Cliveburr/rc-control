#ifndef __MOTOR_DRV8833_H__
#define __MOTOR_DRV8833_H__

#include "project_config.h"
#include "motor_control.h"

#if ENABLE_MOTOR_CONTROL && (MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_DRV8833)

#include <driver/ledc.h>

// =============================================================================
// DRV8833 HARDWARE CONFIGURATION
// =============================================================================

/**
 * @brief GPIO pins for DRV8833 motor driver
 * 
 * The DRV8833 uses two pins (IN1, IN2) for PWM control.
 * Choose GPIO pins that support PWM and don't conflict with other peripherals.
 * 
 * Current pin usage in project:
 * - GPIO 2:  LED Light
 * - GPIO 4:  Servo Control  
 * - GPIO 14: LED Horn
 * - GPIO 34: Battery Monitor (ADC input only)
 * - Camera pins: 0, 19, 21, 26, 27, 32, 35, 36, 39 (when enabled)
 */
#define DRV8833_IN1_PIN         16  ///< GPIO16 for IN1 (motor direction/speed control)
#define DRV8833_IN2_PIN         17  ///< GPIO17 for IN2 (motor direction/speed control)

// =============================================================================
// DRV8833 PWM CONFIGURATION
// =============================================================================

/**
 * @brief LEDC timer configuration for DRV8833 PWM
 */
#define DRV8833_LEDC_TIMER          LEDC_TIMER_2        ///< LEDC timer (avoid conflicts)
#define DRV8833_LEDC_MODE           LEDC_LOW_SPEED_MODE ///< LEDC speed mode
#define DRV8833_LEDC_IN1_CHANNEL    LEDC_CHANNEL_2      ///< LEDC channel for IN1
#define DRV8833_LEDC_IN2_CHANNEL    LEDC_CHANNEL_3      ///< LEDC channel for IN2
#define DRV8833_LEDC_DUTY_RES       LEDC_TIMER_10_BIT   ///< 10-bit resolution (0-1023)
#define DRV8833_LEDC_FREQUENCY      1000                ///< 1kHz PWM frequency

/**
 * @brief DRV8833 PWM duty cycle calculations
 */
#define DRV8833_MAX_DUTY            ((1 << 10) - 1)     ///< Maximum duty cycle (1023 for 10-bit)
#define DRV8833_MIN_DUTY            0                   ///< Minimum duty cycle

// =============================================================================
// DRV8833 CONTROL MODES CONFIGURATION
// =============================================================================

/**
 * @brief DRV8833 control mode settings
 * 
 * The DRV8833 supports different control modes based on IN1/IN2 states:
 * - Forward:  IN1=PWM, IN2=LOW
 * - Reverse:  IN1=LOW, IN2=PWM  
 * - Brake:    IN1=HIGH, IN2=HIGH (configurable)
 * - Free:     IN1=LOW, IN2=LOW (configurable)
 * 
 * These can be customized based on your specific requirements.
 */

/**
 * @brief Brake mode configuration
 * Set to 1 for IN1=HIGH, IN2=HIGH (brake)
 * Set to 0 for IN1=LOW, IN2=LOW (alternative brake mode)
 */
#define DRV8833_BRAKE_MODE_HIGH     1

/**
 * @brief Free running mode configuration  
 * Set to 1 for IN1=HIGH, IN2=HIGH (coast)
 * Set to 0 for IN1=LOW, IN2=LOW (coast)
 */
#define DRV8833_FREE_MODE_LOW       1

// =============================================================================
// DRV8833 DRIVER INTERFACE FUNCTIONS
// =============================================================================

/**
 * @brief Initialize DRV8833 motor driver
 * 
 * Configures GPIO pins and LEDC channels for PWM control.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t drv8833_init(void);

/**
 * @brief Deinitialize DRV8833 motor driver
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t drv8833_deinit(void);

/**
 * @brief Set motor speed using DRV8833
 * 
 * @param speed Speed value from -100 to +100
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t drv8833_set_speed(int speed);

/**
 * @brief Set motor mode using DRV8833
 * 
 * @param mode Motor mode to set
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t drv8833_set_mode(motor_mode_t mode);

/**
 * @brief Stop motor using DRV8833
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t drv8833_stop(void);

/**
 * @brief Get current motor state from DRV8833
 * 
 * @param state Pointer to store current motor state
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t drv8833_get_state(motor_state_t *state);

/**
 * @brief Get DRV8833 driver interface
 * 
 * Returns the driver interface structure for registration with the HAL.
 * 
 * @return Pointer to DRV8833 driver interface
 */
const motor_driver_interface_t* drv8833_get_interface(void);

#endif // ENABLE_MOTOR_CONTROL && (MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_DRV8833)

#endif // __MOTOR_DRV8833_H__