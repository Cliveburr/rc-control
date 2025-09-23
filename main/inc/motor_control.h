#ifndef __MOTOR_CONTROL_H__
#define __MOTOR_CONTROL_H__

#include "project_config.h"

#if ENABLE_MOTOR_CONTROL

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// MOTOR CONTROL TYPES AND ENUMS
// =============================================================================

/**
 * @brief Motor control modes
 */
typedef enum {
    MOTOR_MODE_FORWARD,     ///< Motor rotating forward
    MOTOR_MODE_REVERSE,     ///< Motor rotating in reverse
    MOTOR_MODE_BRAKE,       ///< Motor braking (short circuit)
    MOTOR_MODE_FREE         ///< Motor free running (no power)
} motor_mode_t;

/**
 * @brief Motor control parameters
 */
typedef struct {
    int speed;              ///< Speed value (-100 to +100)
    motor_mode_t mode;      ///< Current motor mode
    bool enabled;           ///< Motor enabled state
} motor_state_t;

/**
 * @brief Motor driver interface
 * 
 * This structure defines the interface that all motor drivers must implement.
 * It provides a hardware abstraction layer (HAL) for different motor controllers.
 */
typedef struct motor_driver_interface {
    const char *name;                                           ///< Driver name
    esp_err_t (*init)(void);                                   ///< Initialize driver
    esp_err_t (*deinit)(void);                                 ///< Deinitialize driver
    esp_err_t (*set_speed)(int speed);                         ///< Set motor speed (-100 to +100)
    esp_err_t (*set_mode)(motor_mode_t mode);                  ///< Set motor mode
    esp_err_t (*stop)(void);                                   ///< Stop motor immediately
    esp_err_t (*get_state)(motor_state_t *state);              ///< Get current motor state
} motor_driver_interface_t;

// =============================================================================
// MOTOR CONTROL API
// =============================================================================

/**
 * @brief Initialize motor control system
 * 
 * This function initializes the motor control HAL and the configured driver.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_init(void);

/**
 * @brief Deinitialize motor control system
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_deinit(void);

/**
 * @brief Set motor speed
 * 
 * @param speed Speed value from -100 to +100
 *              -100 = Full reverse
 *              0    = Stop
 *              +100 = Full forward
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_set_speed(int speed);

/**
 * @brief Set motor mode directly
 * 
 * @param mode Motor mode to set
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_set_mode(motor_mode_t mode);

/**
 * @brief Stop motor immediately
 * 
 * This sets the motor to brake mode for immediate stop.
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_stop(void);

/**
 * @brief Get current motor state
 * 
 * @param state Pointer to store current motor state
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_get_state(motor_state_t *state);

/**
 * @brief Register motor driver
 * 
 * This function is used internally to register the active motor driver.
 * 
 * @param driver Pointer to motor driver interface
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t motor_control_register_driver(const motor_driver_interface_t *driver);

// =============================================================================
// MOTOR DRIVER SELECTION
// =============================================================================

/**
 * @brief Motor driver types
 */
typedef enum {
    MOTOR_DRIVER_DRV8833,   ///< DRV8833 dual H-bridge driver
    MOTOR_DRIVER_L298N,     ///< L298N dual H-bridge driver (future)
    MOTOR_DRIVER_CUSTOM     ///< Custom driver implementation
} motor_driver_type_t;

/**
 * @brief Active motor driver configuration
 * 
 * Change this to switch between different motor drivers.
 * Only one driver can be active at compile time.
 */
#define MOTOR_ACTIVE_DRIVER     MOTOR_DRIVER_DRV8833

// Include the appropriate driver header based on configuration
#if MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_DRV8833
    #include "motor_drv8833.h"
#elif MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_L298N
    #include "motor_l298n.h"  // Future implementation
#endif

#endif // ENABLE_MOTOR_CONTROL

#endif // __MOTOR_CONTROL_H__