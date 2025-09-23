#ifndef __PROJECT_CONFIG_H__
#define __PROJECT_CONFIG_H__

/**
 * @file project_config.h
 * @brief Project feature configuration
 * 
 * This file contains compile-time feature flags for the RC Control project.
 * Enable or disable features by commenting/uncommenting the #define statements.
 */

// =============================================================================
// HARDWARE FEATURE CONFIGURATION
// =============================================================================

/**
 * @brief Enable camera support
 * 
 * Set to 1 to enable camera streaming functionality.
 * Set to 0 to disable camera support (for boards without camera).
 * 
 * Uncomment the line on file idf_component.yml
 *   # espressif/esp32-camera: "*"
 * Uncomment the lines between CONFIGURATION FOR CAM in file sdkconfig
 * 
 * Requirements when enabled:
 * - ESP32-CAM or compatible board with camera module
 * - PSRAM recommended for better performance
 * - esp32-camera component dependency
 */
#define ENABLE_CAMERA_SUPPORT       0   // 0 = Disabled, 1 = Enabled

/**
 * @brief Enable servo motor control
 * 
 * Set to 1 to enable servo motor control for steering.
 * Set to 0 to disable servo support.
 */
#define ENABLE_SERVO_CONTROL        1   // 0 = Disabled, 1 = Enabled

/**
 * @brief Enable LED control
 * 
 * Set to 1 to enable LED control (horn/light indicators).
 * Set to 0 to disable LED support.
 */
#define ENABLE_LED_CONTROL          1   // 0 = Disabled, 1 = Enabled

/**
 * @brief Enable battery monitoring
 * 
 * Set to 1 to enable battery voltage monitoring via ADC.
 * Set to 0 to disable battery monitoring.
 * Configure details in battery_monitor.h
 */
#define ENABLE_BATTERY_MONITORING   1   // 0 = Disabled, 1 = Enabled

/**
 * @brief Enable motor speed control
 * 
 * Set to 1 to enable motor speed control functionality.
 * Set to 0 to disable motor control (for testing without motor).
 */
#define ENABLE_MOTOR_CONTROL        1   // 0 = Disabled, 1 = Enabled

// =============================================================================
// SOFTWARE FEATURE CONFIGURATION
// =============================================================================

/**
 * @brief Enable OTA (Over-The-Air) updates
 * 
 * Set to 1 to enable firmware update via web interface.
 * Set to 0 to disable OTA support.
 */
#define ENABLE_OTA_UPDATES          1   // 0 = Disabled, 1 = Enabled

/**
 * @brief Enable WiFi configuration web interface
 * 
 * Set to 1 to enable WiFi configuration via web interface.
 * Set to 0 to use hardcoded WiFi settings.
 */
#define ENABLE_WIFI_CONFIG          1   // 0 = Disabled, 1 = Enabled

/**
 * @brief Enable debug logging
 * 
 * Set to 1 to enable verbose debug logging.
 * Set to 0 for production builds with minimal logging.
 */
#define ENABLE_DEBUG_LOGGING        1   // 0 = Disabled, 1 = Enabled

// =============================================================================
// VALIDATION AND DERIVED CONFIGURATIONS
// =============================================================================

// Validate camera configuration
#if ENABLE_CAMERA_SUPPORT && !defined(CONFIG_SPIRAM)
    #warning "Camera support enabled but PSRAM not configured. Performance may be limited."
#endif

// Feature dependency validation
#if ENABLE_SERVO_CONTROL && !ENABLE_MOTOR_CONTROL
    #warning "Servo control enabled but motor control disabled. Consider enabling both for full RC functionality."
#endif

// Debug configuration
#if ENABLE_DEBUG_LOGGING
    #define LOG_LEVEL ESP_LOG_DEBUG
#else
    #define LOG_LEVEL ESP_LOG_INFO
#endif

#endif // __PROJECT_CONFIG_H__