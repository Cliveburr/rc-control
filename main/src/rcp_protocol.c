#include "rcp_protocol.h"
#include "http_server.h"
#include "esp_log.h"
#include <string.h>
#include "project_config.h"

#if ENABLE_MOTOR_CONTROL
#include "motor_control.h"
#endif

#if ENABLE_SERVO_CONTROL
#include "servo_control.h"
#endif

#if ENABLE_LED_CONTROL
#include "led_control.h"
#endif

static const char *TAG = "rcp_protocol";

// Forward declarations for handlers
static esp_err_t rcp_handle_motor(const uint8_t* body, size_t len);
static esp_err_t rcp_handle_servo(const uint8_t* body, size_t len);
static esp_err_t rcp_handle_horn(const uint8_t* body, size_t len);
static esp_err_t rcp_handle_light(const uint8_t* body, size_t len);
static esp_err_t rcp_handle_system(const uint8_t* body, size_t len);

// CRC8 lookup table for faster computation


esp_err_t rcp_process_message(uint8_t port, const uint8_t* body, size_t body_len) {
    ESP_LOGD(TAG, "RCP: Received message port=0x%02X, body_len=%zu", port, body_len);

    if (body_len > RCP_MAX_BODY_SIZE) {
        ESP_LOGW(TAG, "RCP: Body length %zu exceeds max %d", body_len, RCP_MAX_BODY_SIZE);
        return RCP_ERR_INVALID_SIZE;
    }

    switch (port) {
        case RCP_PORT_MOTOR:
            return rcp_handle_motor(body, body_len);

        case RCP_PORT_SERVO:
            return rcp_handle_servo(body, body_len);

        case RCP_PORT_HORN:
            return rcp_handle_horn(body, body_len);

        case RCP_PORT_LIGHT:
            return rcp_handle_light(body, body_len);

        case RCP_PORT_SYSTEM:
            return rcp_handle_system(body, body_len);

        default:
            ESP_LOGW(TAG, "RCP: Unknown port 0x%02X", port);
            return RCP_ERR_INVALID_PORT;
    }
}

static esp_err_t rcp_handle_motor(const uint8_t* body, size_t len) {
    if (len != 1) {
        ESP_LOGW(TAG, "RCP: Invalid motor command size %zu (expected 1)", len);
        return RCP_ERR_INVALID_SIZE;
    }

    int8_t speed = (int8_t)body[0];

#if ENABLE_MOTOR_CONTROL
    // Validate speed range
    if (speed < -100 || speed > 100) {
        ESP_LOGW(TAG, "RCP: Motor speed out of range: %d", speed);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Process command
    ESP_LOGI(TAG, "RCP: Motor speed set to %d", speed);

    esp_err_t ret = motor_control_set_speed(speed);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RCP: Failed to set motor speed: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    ESP_LOGW(TAG, "RCP: Motor control disabled in project_config.h (speed=%d ignored)", speed);
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_servo(const uint8_t* body, size_t len) {
    if (len != 1) {
        ESP_LOGW(TAG, "RCP: Invalid servo command size %zu (expected 1)", len);
        return RCP_ERR_INVALID_SIZE;
    }

    int8_t angle = (int8_t)body[0];

#if ENABLE_SERVO_CONTROL
    // Validate angle range
    if (angle < -100 || angle > 100) {
        ESP_LOGW(TAG, "RCP: Servo angle out of range: %d", angle);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Process command
    ESP_LOGI(TAG, "RCP: Servo angle set to %d", angle);

    esp_err_t ret = servo_control_set_position(angle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RCP: Failed to set servo position: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    ESP_LOGW(TAG, "RCP: Servo control disabled in project_config.h (angle=%d ignored)", angle);
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_horn(const uint8_t* body, size_t len) {
    if (len != 1) {
        ESP_LOGW(TAG, "RCP: Invalid horn command size %zu (expected 1)", len);
        return RCP_ERR_INVALID_SIZE;
    }

    uint8_t state = body[0];

    // Validate state
    if (state > 1) {
        ESP_LOGW(TAG, "RCP: Horn state out of range: %d", state);
        return ESP_ERR_INVALID_ARG;
    }
    
#if ENABLE_LED_CONTROL
    // Process command
    ESP_LOGI(TAG, "RCP: Horn %s", state ? "ON" : "OFF");
    led_horn_set(state != 0);
#else
    ESP_LOGW(TAG, "RCP: LED control disabled in project_config.h (horn=%s ignored)", state ? "ON" : "OFF");
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_light(const uint8_t* body, size_t len) {
    if (len != 1) {
        ESP_LOGW(TAG, "RCP: Invalid light command size %zu (expected 1)", len);
        return RCP_ERR_INVALID_SIZE;
    }

    uint8_t state = body[0];

    // Validate state
    if (state > 1) {
        ESP_LOGW(TAG, "RCP: Light state out of range: %d", state);
        return ESP_ERR_INVALID_ARG;
    }
    
#if ENABLE_LED_CONTROL
    // Process command
    ESP_LOGI(TAG, "RCP: Light %s", state ? "ON" : "OFF");
    led_light_set(state != 0);
#else
    ESP_LOGW(TAG, "RCP: LED control disabled in project_config.h (light=%s ignored)", state ? "ON" : "OFF");
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_system(const uint8_t* body, size_t len) {
    if (len != sizeof(rcp_system_body_t)) {
        ESP_LOGW(TAG, "RCP: Invalid system command size %zu (expected %zu)", 
                 len, sizeof(rcp_system_body_t));

        if (len <= 16) {
            char debug_hex[64] = {0};
            for (size_t i = 0; i < len && i < 15; i++) {
                snprintf(debug_hex + i*3, sizeof(debug_hex) - i*3, "%02X ", body[i]);
            }
            ESP_LOGW(TAG, "RCP: Received system data: %s", debug_hex);
        }

        return RCP_ERR_INVALID_SIZE;
    }

    const rcp_system_body_t* cmd = (const rcp_system_body_t*)body;

    ESP_LOGI(TAG, "RCP: System command 0x%02X with param 0x%02X", 
             cmd->command, cmd->param);
    
    // Process system commands
    switch (cmd->command) {
        case RCP_SYS_PING:
            ESP_LOGI(TAG, "RCP: Ping received");
            // Could send pong response here
            break;
            
        case RCP_SYS_RESET:
            ESP_LOGW(TAG, "RCP: Reset command received");
            // Could trigger system reset here
            break;
            
        case RCP_SYS_STATUS:
            ESP_LOGI(TAG, "RCP: Status request received");
            // Could send telemetry response here
            break;
            
        case RCP_SYS_CONFIG:
            ESP_LOGI(TAG, "RCP: Config command received");
            // Could handle configuration here
            break;
            
        default:
            ESP_LOGW(TAG, "RCP: Unknown system command 0x%02X", cmd->command);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    return ESP_OK;
}

esp_err_t rcp_send_response(uint8_t port, const void* body, size_t body_len) {
    if (body_len > RCP_MAX_BODY_SIZE) {
        ESP_LOGW(TAG, "RCP: Response body too large (%zu bytes)", body_len);
        return RCP_ERR_INVALID_SIZE;
    }

    uint8_t frame[RCP_HEADER_SIZE + RCP_MAX_BODY_SIZE] = {0};
    RCP_SET_LENGTH(frame, body_len);
    frame[2] = port;

    if (body_len > 0 && body != NULL) {
        memcpy(frame + RCP_HEADER_SIZE, body, body_len);
    }

    size_t total_len = RCP_HEADER_SIZE + body_len;

    return http_server_broadcast_ws_binary(frame, total_len);
}

esp_err_t rcp_send_battery_status(uint16_t voltage_mv, uint8_t level, uint8_t type) {
    rcp_battery_body_t response = {
        .voltage_mv = voltage_mv,
        .level = level,
        .type = type
    };

    ESP_LOGD(TAG, "RCP: Sending battery status: %umV, level=%u, type=%uS", 
             voltage_mv, level, type);

    return rcp_send_response(RCP_PORT_BATTERY, &response, sizeof(response));
}

esp_err_t rcp_send_telemetry(int8_t speed, int8_t angle, uint8_t horn_state, 
                            uint8_t light_state, uint8_t flags) {
    rcp_telemetry_body_t response = {
        .current_speed = speed,
        .current_angle = angle,
        .horn_state = horn_state,
        .light_state = light_state,
        .flags = flags
    };

    ESP_LOGD(TAG, "RCP: Sending telemetry: speed=%d, angle=%d, horn=%u, light=%u, flags=0x%02X", 
             speed, angle, horn_state, light_state, flags);

    return rcp_send_response(RCP_PORT_TELEMETRY, &response, sizeof(response));
}

esp_err_t rcp_init(void) {
    ESP_LOGI(TAG, "RCP: Protocol initialized");
    return ESP_OK;
}

void rcp_deinit(void) {
    ESP_LOGI(TAG, "RCP: Protocol deinitialized");
}