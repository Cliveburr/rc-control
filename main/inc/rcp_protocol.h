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

// Sync byte for frame synchronization
#define RCP_SYNC_BYTE        0xAA

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

// Maximum payload size (excluding header)
#define RCP_MAX_PAYLOAD_SIZE 16

// Error codes
#define RCP_ERR_INVALID_SYNC    (ESP_ERR_INVALID_ARG + 1)
#define RCP_ERR_INVALID_PORT    (ESP_ERR_INVALID_ARG + 2)
#define RCP_ERR_INVALID_CRC     (ESP_ERR_INVALID_CRC)
#define RCP_ERR_INVALID_SIZE    (ESP_ERR_INVALID_SIZE)

/**
 * @brief Basic RCP header structure
 */
#pragma pack(1)
typedef struct {
    uint8_t sync;      // Synchronization byte (0xAA)
    uint8_t port;      // Port/route for command
} rcp_header_t;
#pragma pack()

/**
 * @brief Motor control command (Port 0x01)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x01
    int8_t speed;         // Motor speed: -100 to +100
    uint8_t checksum;     // CRC8 checksum
} rcp_motor_t;
#pragma pack()

/**
 * @brief Servo control command (Port 0x02)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x02
    int8_t angle;         // Servo angle: -100 to +100
    uint8_t checksum;     // CRC8 checksum
} rcp_servo_t;
#pragma pack()

/**
 * @brief Horn control command (Port 0x03)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x03
    uint8_t state;        // Horn state: 0=OFF, 1=ON
    uint8_t checksum;     // CRC8 checksum
} rcp_horn_t;
#pragma pack()

/**
 * @brief Light control command (Port 0x04)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x04
    uint8_t state;        // Light state: 0=OFF, 1=ON
    uint8_t checksum;     // CRC8 checksum
} rcp_light_t;
#pragma pack()

/**
 * @brief Battery status response (Port 0x80)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x80
    uint16_t voltage_mv;  // Voltage in millivolts
    uint8_t level;        // Battery level 0-10
    uint8_t type;         // Battery type: 1=1S, 2=2S, etc.
    uint8_t checksum;     // CRC8 checksum
} rcp_battery_t;
#pragma pack()

/**
 * @brief Telemetry response (Port 0x81)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x81
    int8_t current_speed; // Current motor speed
    int8_t current_angle; // Current servo angle
    uint8_t horn_state;   // Horn state
    uint8_t light_state;  // Light state
    uint8_t flags;        // Status flags
    uint8_t checksum;     // CRC8 checksum
} rcp_telemetry_t;
#pragma pack()

/**
 * @brief System command (Port 0x10)
 */
#pragma pack(1)
typedef struct {
    rcp_header_t header;  // sync=0xAA, port=0x10
    uint8_t command;      // System command ID
    uint8_t param;        // Command parameter
    uint8_t checksum;     // CRC8 checksum
} rcp_system_t;
#pragma pack()

// System commands
#define RCP_SYS_PING         0x01  // Ping request
#define RCP_SYS_RESET        0x02  // Reset system
#define RCP_SYS_STATUS       0x03  // Request status
#define RCP_SYS_CONFIG       0x04  // Configuration request

/**
 * @brief Calculate CRC8 checksum for RCP message
 * 
 * @param data Pointer to message data
 * @param len Length of message (including checksum byte)
 * @return CRC8 checksum value
 */
uint8_t rcp_calculate_checksum(const void* data, size_t len);

/**
 * @brief Validate RCP message checksum
 * 
 * @param data Pointer to message data
 * @param len Length of message
 * @return true if checksum is valid, false otherwise
 */
bool rcp_validate_checksum(const void* data, size_t len);

/**
 * @brief Process incoming RCP message
 * 
 * @param data Pointer to message data
 * @param len Length of message
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_process_message(const uint8_t* data, size_t len);

/**
 * @brief Send RCP response message
 * 
 * @param data Pointer to response data
 * @param len Length of response
 * @return ESP_OK on success, error code on failure
 */
esp_err_t rcp_send_response(const void* data, size_t len);

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
#define RCP_CREATE_MOTOR_CMD(speed_val) \
    { .header = {RCP_SYNC_BYTE, RCP_PORT_MOTOR}, .speed = (speed_val), .checksum = 0 }

#define RCP_CREATE_SERVO_CMD(angle_val) \
    { .header = {RCP_SYNC_BYTE, RCP_PORT_SERVO}, .angle = (angle_val), .checksum = 0 }

#define RCP_CREATE_HORN_CMD(state_val) \
    { .header = {RCP_SYNC_BYTE, RCP_PORT_HORN}, .state = (state_val), .checksum = 0 }

#define RCP_CREATE_LIGHT_CMD(state_val) \
    { .header = {RCP_SYNC_BYTE, RCP_PORT_LIGHT}, .state = (state_val), .checksum = 0 }

#ifdef __cplusplus
}
#endif

#endif // RCP_PROTOCOL_H