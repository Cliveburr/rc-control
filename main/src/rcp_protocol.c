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
static esp_err_t rcp_handle_motor(const uint8_t* data, size_t len);
static esp_err_t rcp_handle_servo(const uint8_t* data, size_t len);
static esp_err_t rcp_handle_horn(const uint8_t* data, size_t len);
static esp_err_t rcp_handle_light(const uint8_t* data, size_t len);
static esp_err_t rcp_handle_system(const uint8_t* data, size_t len);

// CRC8 lookup table for faster computation
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

uint8_t rcp_calculate_checksum(const void* data, size_t len) {
    if (data == NULL || len == 0) {
        return 0;
    }
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint8_t crc = 0xAA; // Start with sync byte value
    
    // Calculate CRC8 for all bytes except the last one (checksum byte)
    for (size_t i = 0; i < len - 1; i++) {
        crc = crc8_table[crc ^ bytes[i]];
    }
    
    return crc;
}

bool rcp_validate_checksum(const void* data, size_t len) {
    if (data == NULL || len == 0) {
        return false;
    }
    
    const uint8_t* bytes = (const uint8_t*)data;
    uint8_t received_checksum = bytes[len - 1];
    uint8_t calculated_checksum = rcp_calculate_checksum(data, len);
    
    return (received_checksum == calculated_checksum);
}

esp_err_t rcp_process_message(const uint8_t* data, size_t len) {
    if (data == NULL) {
        ESP_LOGW(TAG, "RCP: Null data pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (len < sizeof(rcp_header_t)) {
        ESP_LOGW(TAG, "RCP: Message too short (%zu bytes)", len);
        return RCP_ERR_INVALID_SIZE;
    }
    
    const rcp_header_t* header = (const rcp_header_t*)data;
    
    // Validate sync byte
    if (header->sync != RCP_SYNC_BYTE) {
        ESP_LOGW(TAG, "RCP: Invalid sync byte 0x%02X (expected 0x%02X)", 
                 header->sync, RCP_SYNC_BYTE);
        return RCP_ERR_INVALID_SYNC;
    }
    
    // Log received message for debugging
    ESP_LOGD(TAG, "RCP: Received message port=0x%02X, len=%zu", header->port, len);
    
    // Route to appropriate handler based on port
    switch (header->port) {
        case RCP_PORT_MOTOR:
            return rcp_handle_motor(data, len);
            
        case RCP_PORT_SERVO:
            return rcp_handle_servo(data, len);
            
        case RCP_PORT_HORN:
            return rcp_handle_horn(data, len);
            
        case RCP_PORT_LIGHT:
            return rcp_handle_light(data, len);
            
        case RCP_PORT_SYSTEM:
            return rcp_handle_system(data, len);
            
        default:
            ESP_LOGW(TAG, "RCP: Unknown port 0x%02X", header->port);
            return RCP_ERR_INVALID_PORT;
    }
}

static esp_err_t rcp_handle_motor(const uint8_t* data, size_t len) {
    if (len != sizeof(rcp_motor_t)) {
        ESP_LOGW(TAG, "RCP: Invalid motor command size %zu (expected %zu)", 
                 len, sizeof(rcp_motor_t));
        return RCP_ERR_INVALID_SIZE;
    }
    
    const rcp_motor_t* cmd = (const rcp_motor_t*)data;
    
    // Validate checksum
    if (!rcp_validate_checksum(data, len)) {
        ESP_LOGW(TAG, "RCP: Motor command checksum mismatch");
        return RCP_ERR_INVALID_CRC;
    }
    
#if ENABLE_MOTOR_CONTROL
    // Validate speed range
    if (cmd->speed < -100 || cmd->speed > 100) {
        ESP_LOGW(TAG, "RCP: Motor speed out of range: %d", cmd->speed);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Process command
    ESP_LOGI(TAG, "RCP: Motor speed set to %d", cmd->speed);
    
    esp_err_t ret = motor_control_set_speed(cmd->speed);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RCP: Failed to set motor speed: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    ESP_LOGW(TAG, "RCP: Motor control disabled in project_config.h (speed=%d ignored)", cmd->speed);
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_servo(const uint8_t* data, size_t len) {
    if (len != sizeof(rcp_servo_t)) {
        ESP_LOGW(TAG, "RCP: Invalid servo command size %zu (expected %zu)", 
                 len, sizeof(rcp_servo_t));
        return RCP_ERR_INVALID_SIZE;
    }
    
    const rcp_servo_t* cmd = (const rcp_servo_t*)data;
    
    // Validate checksum
    if (!rcp_validate_checksum(data, len)) {
        ESP_LOGW(TAG, "RCP: Servo command checksum mismatch");
        return RCP_ERR_INVALID_CRC;
    }
    
#if ENABLE_SERVO_CONTROL
    // Validate angle range
    if (cmd->angle < -100 || cmd->angle > 100) {
        ESP_LOGW(TAG, "RCP: Servo angle out of range: %d", cmd->angle);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Process command
    ESP_LOGI(TAG, "RCP: Servo angle set to %d", cmd->angle);
    
    esp_err_t ret = servo_control_set_position(cmd->angle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RCP: Failed to set servo position: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    ESP_LOGW(TAG, "RCP: Servo control disabled in project_config.h (angle=%d ignored)", cmd->angle);
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_horn(const uint8_t* data, size_t len) {
    if (len != sizeof(rcp_horn_t)) {
        ESP_LOGW(TAG, "RCP: Invalid horn command size %zu (expected %zu)", 
                 len, sizeof(rcp_horn_t));
        return RCP_ERR_INVALID_SIZE;
    }
    
    const rcp_horn_t* cmd = (const rcp_horn_t*)data;
    
    // Validate checksum
    if (!rcp_validate_checksum(data, len)) {
        ESP_LOGW(TAG, "RCP: Horn command checksum mismatch");
        return RCP_ERR_INVALID_CRC;
    }
    
    // Validate state
    if (cmd->state > 1) {
        ESP_LOGW(TAG, "RCP: Horn state out of range: %d", cmd->state);
        return ESP_ERR_INVALID_ARG;
    }
    
#if ENABLE_LED_CONTROL
    // Process command
    ESP_LOGI(TAG, "RCP: Horn %s", cmd->state ? "ON" : "OFF");
    led_horn_set(cmd->state != 0);
#else
    ESP_LOGW(TAG, "RCP: LED control disabled in project_config.h (horn=%s ignored)", cmd->state ? "ON" : "OFF");
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_light(const uint8_t* data, size_t len) {
    if (len != sizeof(rcp_light_t)) {
        ESP_LOGW(TAG, "RCP: Invalid light command size %zu (expected %zu)", 
                 len, sizeof(rcp_light_t));
        return RCP_ERR_INVALID_SIZE;
    }
    
    const rcp_light_t* cmd = (const rcp_light_t*)data;
    
    // Validate checksum
    if (!rcp_validate_checksum(data, len)) {
        ESP_LOGW(TAG, "RCP: Light command checksum mismatch");
        return RCP_ERR_INVALID_CRC;
    }
    
    // Validate state
    if (cmd->state > 1) {
        ESP_LOGW(TAG, "RCP: Light state out of range: %d", cmd->state);
        return ESP_ERR_INVALID_ARG;
    }
    
#if ENABLE_LED_CONTROL
    // Process command
    ESP_LOGI(TAG, "RCP: Light %s", cmd->state ? "ON" : "OFF");
    led_light_set(cmd->state != 0);
#else
    ESP_LOGW(TAG, "RCP: LED control disabled in project_config.h (light=%s ignored)", cmd->state ? "ON" : "OFF");
#endif
    
    return ESP_OK;
}

static esp_err_t rcp_handle_system(const uint8_t* data, size_t len) {
    if (len != sizeof(rcp_system_t)) {
        ESP_LOGW(TAG, "RCP: Invalid system command size %zu (expected %zu)", 
                 len, sizeof(rcp_system_t));
        return RCP_ERR_INVALID_SIZE;
    }
    
    const rcp_system_t* cmd = (const rcp_system_t*)data;
    
    // Validate checksum
    if (!rcp_validate_checksum(data, len)) {
        ESP_LOGW(TAG, "RCP: System command checksum mismatch");
        return RCP_ERR_INVALID_CRC;
    }
    
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

esp_err_t rcp_send_response(const void* data, size_t len) {
    if (data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use existing WebSocket broadcast function
    // Note: This assumes WebSocket binary frame support
    return http_server_broadcast_ws_binary(data, len);
}

esp_err_t rcp_send_battery_status(uint16_t voltage_mv, uint8_t level, uint8_t type) {
    rcp_battery_t response = {
        .header = {RCP_SYNC_BYTE, RCP_PORT_BATTERY},
        .voltage_mv = voltage_mv,
        .level = level,
        .type = type,
        .checksum = 0
    };
    
    // Calculate and set checksum
    response.checksum = rcp_calculate_checksum(&response, sizeof(response));
    
    ESP_LOGD(TAG, "RCP: Sending battery status: %umV, level=%u, type=%uS", 
             voltage_mv, level, type);
    
    return rcp_send_response(&response, sizeof(response));
}

esp_err_t rcp_send_telemetry(int8_t speed, int8_t angle, uint8_t horn_state, 
                            uint8_t light_state, uint8_t flags) {
    rcp_telemetry_t response = {
        .header = {RCP_SYNC_BYTE, RCP_PORT_TELEMETRY},
        .current_speed = speed,
        .current_angle = angle,
        .horn_state = horn_state,
        .light_state = light_state,
        .flags = flags,
        .checksum = 0
    };
    
    // Calculate and set checksum
    response.checksum = rcp_calculate_checksum(&response, sizeof(response));
    
    ESP_LOGD(TAG, "RCP: Sending telemetry: speed=%d, angle=%d, horn=%u, light=%u, flags=0x%02X", 
             speed, angle, horn_state, light_state, flags);
    
    return rcp_send_response(&response, sizeof(response));
}

esp_err_t rcp_init(void) {
    ESP_LOGI(TAG, "RCP: Protocol v%d.%d initialized", 
             RCP_VERSION_MAJOR, RCP_VERSION_MINOR);
    return ESP_OK;
}

void rcp_deinit(void) {
    ESP_LOGI(TAG, "RCP: Protocol deinitialized");
}