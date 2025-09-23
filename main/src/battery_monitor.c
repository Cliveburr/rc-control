#include "battery_monitor.h"

#if ENABLE_BATTERY_MONITORING

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_http_server.h>
#include "http_server.h"
#include "rcp_protocol.h"

static const char *TAG = "battery_monitor";

// Configuration using defines from battery_monitor.h
static battery_config_t battery_config = {
    .enabled = BATTERY_MONITORING_ENABLED,
    .adc_channel = BATTERY_ADC_CHANNEL,
    .resistor_r1 = BATTERY_RESISTOR_R1,
    .resistor_r2 = BATTERY_RESISTOR_R2,
    .battery_type = (BATTERY_TYPE == 1) ? BATTERY_TYPE_1S : BATTERY_TYPE_2S,
    .read_interval_ms = BATTERY_READ_INTERVAL_MS
};

// ADC handles
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool adc_calibrated = false;

// Task handles
static TaskHandle_t battery_task_handle = NULL;
static bool battery_task_running = false;

// WebSocket handle for sending data - will be implemented later
// static httpd_handle_t ws_server_handle = NULL;
// static int ws_client_fd = -1;  // Simple single client tracking for now

/**
 * @brief Initialize ADC calibration
 */
static esp_err_t battery_adc_calibration_init(void)
{
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme version is Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    adc_calibrated = calibrated;
    if (calibrated) {
        ESP_LOGI(TAG, "ADC calibration initialized");
    } else {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
    }

    return ret;
}

esp_err_t battery_monitor_init(void)
{
    if (!battery_config.enabled) {
        ESP_LOGI(TAG, "Battery monitoring disabled");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing battery monitor");
    ESP_LOGI(TAG, "ADC Channel: %d", battery_config.adc_channel);
    ESP_LOGI(TAG, "Resistor R1: %lu ohms", battery_config.resistor_r1);
    ESP_LOGI(TAG, "Resistor R2: %lu ohms", battery_config.resistor_r2);
    ESP_LOGI(TAG, "Battery Type: %dS", battery_config.battery_type);
    
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,  // For 3.3V reference, allows up to ~3.1V input
    };
    
    ret = adc_oneshot_config_channel(adc1_handle, battery_config.adc_channel, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize calibration
    battery_adc_calibration_init();

    ESP_LOGI(TAG, "Battery monitor initialized successfully");
    return ESP_OK;
}

esp_err_t battery_get_voltage(float *voltage)
{
    if (!battery_config.enabled || adc1_handle == NULL || voltage == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    int adc_raw = 0;
    esp_err_t ret = adc_oneshot_read(adc1_handle, battery_config.adc_channel, &adc_raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    int voltage_mv = 0;
    if (adc_calibrated) {
        ret = adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to convert ADC to voltage: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        // Fallback calculation without calibration
        // ESP32 ADC: 4095 counts = ~3100mV with 11dB attenuation
        voltage_mv = (adc_raw * 3100) / 4095;
    }

    // Convert to actual battery voltage using voltage divider formula
    // Vbat = Vadc * (R1 + R2) / R2
    float adc_voltage = voltage_mv / 1000.0; // Convert mV to V
    float battery_voltage = adc_voltage * (battery_config.resistor_r1 + battery_config.resistor_r2) / battery_config.resistor_r2;
    
    *voltage = battery_voltage;

    ESP_LOGD(TAG, "ADC Raw: %d, ADC Voltage: %.3fV, Battery Voltage: %.3fV", 
             adc_raw, adc_voltage, battery_voltage);

    return ESP_OK;
}

battery_type_t battery_get_type(void)
{
    return battery_config.battery_type;
}

esp_err_t battery_send_voltage(float voltage)
{
    httpd_handle_t server = (httpd_handle_t)http_server_get_handle();
    if (server == NULL) {
        ESP_LOGW(TAG, "WebSocket server not available");
        return ESP_ERR_INVALID_STATE;
    }

    // Convert voltage to millivolts for transmission
    uint16_t voltage_mv = (uint16_t)(voltage * 1000.0f);
    
    // Calculate battery level (0-10)
    uint8_t level = 0;
    if (battery_config.battery_type == BATTERY_TYPE_1S) {
        // 1S LiPo: 3.0V-4.2V
        float percentage = (voltage - 3.0f) / (4.2f - 3.0f) * 100.0f;
        level = (uint8_t)(percentage / 10.0f);
    } else {
        // 2S LiPo: 6.0V-8.4V  
        float percentage = (voltage - 6.0f) / (8.4f - 6.0f) * 100.0f;
        level = (uint8_t)(percentage / 10.0f);
    }
    level = (level > 10) ? 10 : level;
    
    // Create RCP battery response: [sync][port][voltage_mv_low][voltage_mv_high][level][type][checksum]
    uint8_t response[7];
    response[0] = 0xAA;  // Sync byte
    response[1] = 0x80;  // Battery response port
    response[2] = voltage_mv & 0xFF;        // Voltage low byte
    response[3] = (voltage_mv >> 8) & 0xFF; // Voltage high byte  
    response[4] = level;                    // Battery level (0-10)
    response[5] = battery_config.battery_type; // Battery type (1S or 2S)
    
    // Calculate CRC8 checksum
    response[6] = rcp_calculate_checksum(response, sizeof(response));
    
    // Send binary message via WebSocket broadcast
    esp_err_t ret = http_server_broadcast_ws_binary(response, sizeof(response));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "RCP battery message broadcasted: %.3fV, level=%d/10, type=%dS", voltage, level, battery_config.battery_type);
    } else {
        ESP_LOGW(TAG, "Failed to broadcast RCP battery message: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t battery_send_init_message(void *req)
{
    if (!battery_config.enabled) {
        return ESP_OK;
    }

    // Send initial battery reading immediately via RCP
    float voltage = 0.0f;
    esp_err_t ret = battery_get_voltage(&voltage);
    if (ret == ESP_OK) {
        ret = battery_send_voltage(voltage);
        ESP_LOGI(TAG, "RCP battery init: %.3fV, type=%dS", voltage, battery_config.battery_type);
    } else {
        ESP_LOGW(TAG, "Failed to read initial battery voltage: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Battery monitoring task
 */
static void battery_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Battery monitoring task started");
    
    int cleanup_counter = 0;
    const int CLEANUP_INTERVAL = 10; // Clean WebSocket clients every 10 battery readings
    
    while (battery_task_running) {
        float voltage = 0.0;
        esp_err_t ret = battery_get_voltage(&voltage);
        
        if (ret == ESP_OK) {
            battery_send_voltage(voltage);
        } else {
            ESP_LOGE(TAG, "Failed to read battery voltage: %s", esp_err_to_name(ret));
        }
        
        // Periodic WebSocket client cleanup
        cleanup_counter++;
        if (cleanup_counter >= CLEANUP_INTERVAL) {
            http_server_cleanup_ws_clients();
            cleanup_counter = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(battery_config.read_interval_ms));
    }
    
    ESP_LOGI(TAG, "Battery monitoring task stopped");
    battery_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t battery_monitor_start_task(void)
{
    if (!battery_config.enabled) {
        ESP_LOGI(TAG, "Battery monitoring disabled, not starting task");
        return ESP_OK;
    }
    
    if (battery_task_running) {
        ESP_LOGW(TAG, "Battery monitoring task already running");
        return ESP_OK;
    }
    
    battery_task_running = true;
    
    BaseType_t ret = xTaskCreate(
        battery_monitor_task,
        "battery_monitor",
        4096,                   // Stack size
        NULL,                   // Parameters
        5,                      // Priority
        &battery_task_handle    // Task handle
    );
    
    if (ret != pdPASS) {
        battery_task_running = false;
        ESP_LOGE(TAG, "Failed to create battery monitoring task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Battery monitoring task started");
    return ESP_OK;
}

void battery_monitor_stop_task(void)
{
    if (battery_task_running) {
        battery_task_running = false;
        ESP_LOGI(TAG, "Stopping battery monitoring task");
    }
}

#endif // ENABLE_BATTERY_MONITORING