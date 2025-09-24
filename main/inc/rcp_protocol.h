#ifndef RCP_PROTOCOL_H
#define RCP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Protocol version
#define RCP_VERSION_MAJOR    1
#define RCP_VERSION_MINOR    0

// RCP frame header: [len_lo][len_hi][port]
#define RCP_HEADER_SIZE      3

// Maximum payload/body size (tunable depending on resources)
#define RCP_MAX_BODY_SIZE    256

// Port definitions
// Control Commands (0x01-0x0F)
#define RCP_PORT_MOTOR       0x01  // Motor speed control
#define RCP_PORT_SERVO       0x02  // Servo steering control
#define RCP_PORT_HORN        0x03  // Horn on/off
#define RCP_PORT_LIGHT       0x04  // Light on/off

// System Commands (0x10-0x1F)
#define RCP_PORT_SYSTEM      0x10  // System commands
#define RCP_PORT_CONFIG      0x11  // Configuration
#define RCP_PORT_STATUS      0x12  // Status requests

// Response Commands (0x80-0xFF)
#define RCP_PORT_BATTERY     0x80  // Battery status response
#define RCP_PORT_TELEMETRY   0x81  // Telemetry data response
#define RCP_PORT_ACK         0xFF  // Acknowledgment

// Reserved/Invalid
#define RCP_PORT_INVALID     0x00  // Invalid/reserved port

// Error codes
#define RCP_ERR_INVALID_PORT    (ESP_ERR_INVALID_ARG + 1)
#define RCP_ERR_INVALID_SIZE    (ESP_ERR_INVALID_SIZE)

/**
 * @brief Battery status response payload (Port 0x80)
 */
#pragma pack(1)
typedef struct {
    uint16_t voltage_mv;  // Voltage in millivolts
    uint8_t level;        // Battery level 0-10
    uint8_t type;         // Battery type: 1=1S, 2=2S, etc.
} rcp_battery_body_t;
#pragma pack()

/**
 * @brief Telemetry response payload (Port 0x81)
 */
#pragma pack(1)
typedef struct {
    int8_t current_speed; // Current motor speed
    int8_t current_angle; // Current servo angle
    uint8_t horn_state;   // Horn state
    uint8_t light_state;  // Light state
    uint8_t flags;        // Status flags
} rcp_telemetry_body_t;
#pragma pack()

/**
 * @brief System command payload (Port 0x10)
 */
#pragma pack(1)
typedef struct {
    uint8_t command;      // System command ID
    uint8_t param;        // Command parameter
} rcp_system_body_t;
#pragma pack()

// System commands
#define RCP_SYS_PING         0x01  // Ping request
#define RCP_SYS_RESET        0x02  // Reset system
#define RCP_SYS_STATUS       0x03  // Request status
#define RCP_SYS_CONFIG       0x04  // Configuration request



/**
 * @brief Process incoming RCP message
 * 
 * @param data Pointer to message data
 * @param len Length of message
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_process_message(uint8_t port, const uint8_t* body, size_t body_len);

/**
 * @brief Send RCP response message
 * 
 * @param port Response port
 * @param body Pointer to payload data
 * @param body_len Length of payload
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_send_response(uint8_t port, const void* body, size_t body_len);

/**
 * @brief Create and send battery status response
 * 
 * @param voltage_mv Battery voltage in millivolts
 * @param level Battery level (0-10)
 * @param type Battery type (1=1S, 2=2S, etc.)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_send_battery_status(uint16_t voltage_mv, uint8_t level, uint8_t type);

/**
 * @brief Create and send telemetry response
 * 
 * @param speed Current motor speed
 * @param angle Current servo angle
 * @param horn_state Horn state
 * @param light_state Light state
 * @param flags Status flags
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_send_telemetry(int8_t speed, int8_t angle, uint8_t horn_state, uint8_t light_state, uint8_t flags);

/**
 * @brief Initialize RCP protocol
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_init(void);

/**
 * @brief Deinitialize RCP protocol
 */
void rcp_deinit(void);

// Convenience macros for creating commands
// Helper macro to compose little-endian length field
#define RCP_SET_LENGTH(buf, len)        \
    do {                                \
        (buf)[0] = (uint8_t)((len) & 0xFF);      \
        (buf)[1] = (uint8_t)(((len) >> 8) & 0xFF); \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // RCP_PROTOCOL_H