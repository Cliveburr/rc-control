#ifndef __BATTERY_MONITOR_H__
#define __BATTERY_MONITOR_H__

#include "project_config.h"

#if ENABLE_BATTERY_MONITORING

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_err.h>

// =============================================================================
// BATTERY MONITORING CONFIGURATION
// =============================================================================

/**
 * @brief Battery monitoring enable/disable
 * Set to 1 to enable, 0 to disable
 */
#define BATTERY_MONITORING_ENABLED      1

/**
 * @brief ADC channel for battery voltage reading
 * ESP32 ADC1 channels:
 * 0 = GPIO36, 1 = GPIO37, 2 = GPIO38, 3 = GPIO39
 * 4 = GPIO32, 5 = GPIO33, 6 = GPIO34, 7 = GPIO35
 */
#define BATTERY_ADC_CHANNEL             ADC_CHANNEL_6   // GPIO34

/**
 * @brief Voltage divider resistor values (in ohms)
 * R1 = Upper resistor (connected to battery+)
 * R2 = Lower resistor (connected to GND)
 * Voltage divider formula: Vout = Vin * R2 / (R1 + R2)
 * 
 * With 222kΩ + 100kΩ:
 * - Current consumption: ~12µA (ultra low power)
 * - Division ratio: 0.311 (Vout = Vin * 0.311)
 * - 1S LiPo: 3.0V-4.2V → 0.93V-1.31V ADC ✅ Perfect
 * - 2S LiPo: 6.0V-8.4V → 1.87V-2.61V ADC ✅ Perfect
 * - Excellent range for both battery types!
 */
#define BATTERY_RESISTOR_R1             222000  // 222kΩ
#define BATTERY_RESISTOR_R2             100000  // 100kΩ

/**
 * @brief Battery type configuration
 * 1 = 1S LiPo (3.7V nominal, 3.0V-4.2V range)
 * 2 = 2S LiPo (7.4V nominal, 6.0V-8.4V range)
 */
#define BATTERY_TYPE                    2       // 1S LiPo

/**
 * @brief Battery reading interval in milliseconds
 * How often to read and send battery voltage
 */
#define BATTERY_READ_INTERVAL_MS        1000    // 1 second

// =============================================================================
// BATTERY MONITORING API
// =============================================================================

/**
 * @brief Battery type configuration
 */
typedef enum {
    BATTERY_TYPE_1S = 1,    // Single cell LiPo (3.7V nominal)
    BATTERY_TYPE_2S = 2     // Two cell LiPo (7.4V nominal)
} battery_type_t;

/**
 * @brief Battery monitoring configuration structure
 */
typedef struct {
    bool enabled;                   // Enable/disable battery monitoring
    adc_channel_t adc_channel;      // ADC channel for battery voltage reading
    uint32_t resistor_r1;           // Upper resistor value in ohms (connected to battery+)
    uint32_t resistor_r2;           // Lower resistor value in ohms (connected to GND)
    battery_type_t battery_type;    // Battery type (1S or 2S)
    uint32_t read_interval_ms;      // Reading interval in milliseconds
} battery_config_t;

/**
 * @brief Initialize battery monitoring
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t battery_monitor_init(void);

/**
 * @brief Get current battery voltage in volts
 * 
 * @param[out] voltage Pointer to store the voltage value
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t battery_get_voltage(float *voltage);

/**
 * @brief Get battery type configuration
 * 
 * @return Battery type (1S or 2S)
 */
battery_type_t battery_get_type(void);

/**
 * @brief Start battery monitoring task
 * 
 * This function starts a FreeRTOS task that reads battery voltage
 * periodically and sends it via WebSocket.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t battery_monitor_start_task(void);

/**
 * @brief Stop battery monitoring task
 */
void battery_monitor_stop_task(void);

/**
 * @brief Send battery initialization message via WebSocket
 * 
 * @param req HTTP request handle for WebSocket
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t battery_send_init_message(void *req);

/**
 * @brief Send battery voltage via WebSocket
 * 
 * @param voltage Battery voltage in volts
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t battery_send_voltage(float voltage);

#endif // ENABLE_BATTERY_MONITORING

#endif // __BATTERY_MONITOR_H__