#include "project_config.h"

#if ENABLE_MOTOR_CONTROL

#include <esp_log.h>
#include <esp_err.h>
#include <string.h>
#include "motor_control.h"

static const char *TAG = "motor_control";
static const motor_driver_interface_t *active_driver = NULL;
static motor_state_t current_state = {
    .speed = 0,
    .mode = MOTOR_MODE_FREE,
    .enabled = false
};
static bool motor_initialized = false;

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

/**
 * @brief Validate speed parameter
 * @param speed Speed value to validate
 * @return true if valid, false otherwise
 */
static bool validate_speed(int speed)
{
    return (speed >= -100 && speed <= 100);
}

/**
 * @brief Convert speed to motor mode and absolute speed
 * @param speed Input speed (-100 to +100)
 * @param mode Output motor mode
 * @param abs_speed Output absolute speed (0 to 100)
 */
static void speed_to_mode(int speed, motor_mode_t *mode, int *abs_speed)
{
    // Validate required parameter
    if (!mode) {
        ESP_LOGE(TAG, "speed_to_mode: mode parameter cannot be NULL");
        return;
    }
    
    if (speed > 0) {
        *mode = MOTOR_MODE_FORWARD;
        if (abs_speed) *abs_speed = speed;
    } else if (speed < 0) {
        *mode = MOTOR_MODE_REVERSE;
        if (abs_speed) *abs_speed = -speed;
    } else {
        *mode = MOTOR_MODE_BRAKE;  // Use brake for stop
        if (abs_speed) *abs_speed = 0;
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

esp_err_t motor_control_init(void)
{
    if (motor_initialized) {
        ESP_LOGW(TAG, "Motor control already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing motor control HAL");

    // Get the appropriate driver interface based on configuration
#if MOTOR_ACTIVE_DRIVER == MOTOR_DRIVER_DRV8833
    active_driver = drv8833_get_interface();
#else
    ESP_LOGE(TAG, "No motor driver configured");
    return ESP_ERR_NOT_SUPPORTED;
#endif

    if (active_driver == NULL) {
        ESP_LOGE(TAG, "Failed to get motor driver interface");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate required driver functions
    if (!active_driver->init || !active_driver->name) {
        ESP_LOGE(TAG, "Driver interface incomplete - missing required functions");
        active_driver = NULL;
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Using motor driver: %s", active_driver->name);

    // Initialize the driver
    esp_err_t ret = active_driver->init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize motor driver: %s", esp_err_to_name(ret));
        active_driver = NULL;
        return ret;
    }

    // Initialize motor state
    current_state.speed = 0;
    current_state.mode = MOTOR_MODE_FREE;
    current_state.enabled = true;

    motor_initialized = true;
    ESP_LOGI(TAG, "Motor control HAL initialized successfully");

    return ESP_OK;
}

esp_err_t motor_control_deinit(void)
{
    if (!motor_initialized) {
        ESP_LOGW(TAG, "Motor control not initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing motor control HAL");

    if (active_driver && active_driver->deinit) {
        esp_err_t ret = active_driver->deinit();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Warning: Driver deinit failed: %s", esp_err_to_name(ret));
        }
    }

    active_driver = NULL;
    current_state.enabled = false;
    motor_initialized = false;

    ESP_LOGI(TAG, "Motor control HAL deinitialized");
    return ESP_OK;
}

esp_err_t motor_control_set_speed(int speed)
{
    if (!motor_initialized || !active_driver) {
        ESP_LOGE(TAG, "Motor control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!validate_speed(speed)) {
        ESP_LOGE(TAG, "Invalid speed value: %d (must be -100 to +100)", speed);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Setting motor speed: %d", speed);

    // Use driver's set_speed if available, otherwise convert to mode
    esp_err_t ret;
    if (active_driver->set_speed) {
        ret = active_driver->set_speed(speed);
    } else {
        // Fallback: convert speed to mode
        motor_mode_t mode;
        int abs_speed;
        speed_to_mode(speed, &mode, &abs_speed);
        ret = active_driver->set_mode(mode);
    }

    if (ret == ESP_OK) {
        current_state.speed = speed;
        speed_to_mode(speed, &current_state.mode, NULL);
    } else {
        ESP_LOGE(TAG, "Failed to set motor speed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t motor_control_set_mode(motor_mode_t mode)
{
    if (!motor_initialized || !active_driver) {
        ESP_LOGE(TAG, "Motor control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!active_driver->set_mode) {
        ESP_LOGE(TAG, "Driver does not support set_mode");
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(TAG, "Setting motor mode: %d", mode);

    esp_err_t ret = active_driver->set_mode(mode);
    if (ret == ESP_OK) {
        current_state.mode = mode;
        // Reset speed when setting mode directly
        if (mode == MOTOR_MODE_BRAKE || mode == MOTOR_MODE_FREE) {
            current_state.speed = 0;
        }
    } else {
        ESP_LOGE(TAG, "Failed to set motor mode: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t motor_control_stop(void)
{
    if (!motor_initialized || !active_driver) {
        ESP_LOGE(TAG, "Motor control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping motor");

    esp_err_t ret;
    if (active_driver->stop) {
        ret = active_driver->stop();
    } else {
        // Fallback to brake mode
        ret = active_driver->set_mode(MOTOR_MODE_BRAKE);
    }

    if (ret == ESP_OK) {
        current_state.speed = 0;
        current_state.mode = MOTOR_MODE_BRAKE;
    } else {
        ESP_LOGE(TAG, "Failed to stop motor: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t motor_control_get_state(motor_state_t *state)
{
    if (state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!motor_initialized || !active_driver) {
        ESP_LOGE(TAG, "Motor control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Try to get state from driver first
    if (active_driver->get_state) {
        esp_err_t ret = active_driver->get_state(state);
        if (ret == ESP_OK) {
            return ret;
        }
        // Fall through to use cached state if driver fails
    }

    // Use cached state
    memcpy(state, &current_state, sizeof(motor_state_t));
    return ESP_OK;
}

esp_err_t motor_control_register_driver(const motor_driver_interface_t *driver)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (motor_initialized) {
        ESP_LOGE(TAG, "Cannot register driver while motor control is initialized");
        return ESP_ERR_INVALID_STATE;
    }

    active_driver = driver;
    ESP_LOGI(TAG, "Registered motor driver: %s", driver->name);
    return ESP_OK;
}

#endif // ENABLE_MOTOR_CONTROL