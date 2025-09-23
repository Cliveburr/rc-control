# Motor Control System Documentation

## Overview

This document describes the motor control system implemented for the RC Control project. The system provides a Hardware Abstraction Layer (HAL) that supports multiple motor drivers with easy driver switching.

## Features

- **Hardware Abstraction Layer (HAL)**: Easily switch between different motor drivers
- **DRV8833 Driver**: Full implementation with PWM control
- **Multiple Control Modes**: Forward, Reverse, Brake, and Free running
- **Speed Control**: -100 to +100 range (full reverse to full forward)
- **Web Interface Integration**: Control via HTTP commands
- **Status Monitoring**: Real-time motor state via system info endpoint
- **Configurable Parameters**: GPIO pins, PWM settings, and control modes

## Architecture

### Motor Control HAL (`motor_control.h/c`)

The main HAL provides a unified interface for all motor drivers:

```c
// Initialize motor control system
esp_err_t motor_control_init(void);

// Set motor speed (-100 to +100)
esp_err_t motor_control_set_speed(int speed);

// Set motor mode directly
esp_err_t motor_control_set_mode(motor_mode_t mode);

// Stop motor immediately
esp_err_t motor_control_stop(void);

// Get current motor state
esp_err_t motor_control_get_state(motor_state_t *state);
```

### DRV8833 Driver (`motor_drv8833.h/c`)

Specific implementation for DRV8833 dual H-bridge motor driver:

- **GPIO Pins**: GPIO16 (IN1), GPIO17 (IN2)
- **PWM Frequency**: 1kHz (configurable)
- **Resolution**: 10-bit (0-1023)
- **Control Method**: Direct PWM on IN1/IN2 pins

## Configuration

### Hardware Configuration (`motor_drv8833.h`)

```c
// GPIO pins for DRV8833
#define DRV8833_IN1_PIN         16  // GPIO16 for IN1
#define DRV8833_IN2_PIN         17  // GPIO17 for IN2

// PWM configuration
#define DRV8833_LEDC_FREQUENCY      1000                // 1kHz PWM frequency
#define DRV8833_LEDC_DUTY_RES       LEDC_TIMER_10_BIT   // 10-bit resolution

// Control modes
#define DRV8833_BRAKE_MODE_HIGH     1  // 1 = IN1=HIGH, IN2=HIGH for brake
#define DRV8833_FREE_MODE_LOW       1  // 1 = IN1=LOW, IN2=LOW for free
```

### Driver Selection (`motor_control.h`)

```c
// Change this to switch motor drivers
#define MOTOR_ACTIVE_DRIVER     MOTOR_DRIVER_DRV8833
```

## Motor Control Modes

| Mode | Description | DRV8833 Configuration |
|------|-------------|----------------------|
| **MOTOR_MODE_FORWARD** | Motor rotating forward | IN1=PWM, IN2=LOW |
| **MOTOR_MODE_REVERSE** | Motor rotating in reverse | IN1=LOW, IN2=PWM |
| **MOTOR_MODE_BRAKE** | Motor braking (short circuit) | IN1=HIGH, IN2=HIGH |
| **MOTOR_MODE_FREE** | Motor free running (no power) | IN1=LOW, IN2=LOW |

## Web Interface Integration

### Speed Control Command

The motor responds to speed commands sent via WebSocket:

```json
{
  "command": "speed",
  "value": 75
}
```

Where `value` ranges from -100 to +100:
- **-100**: Full reverse
- **0**: Stop (brake mode)
- **+100**: Full forward

### System Status

Motor status is included in the `/api/system/info` endpoint:

```json
{
  "motor": {
    "enabled": true,
    "speed": 75,
    "mode": 0,
    "mode_str": "forward",
    "active": true
  }
}
```

## Usage Examples

### Basic Motor Control

```c
#include "motor_control.h"

void app_main(void) {
    // Initialize motor control
    motor_control_init();
    
    // Set forward speed at 50%
    motor_control_set_speed(50);
    
    // Wait 2 seconds
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Set reverse speed at 30%
    motor_control_set_speed(-30);
    
    // Wait 2 seconds
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // Stop motor
    motor_control_stop();
}
```

### Direct Mode Control

```c
// Set motor to free running mode
motor_control_set_mode(MOTOR_MODE_FREE);

// Set motor to brake mode
motor_control_set_mode(MOTOR_MODE_BRAKE);
```

### Get Motor Status

```c
motor_state_t state;
if (motor_control_get_state(&state) == ESP_OK) {
    printf("Speed: %d, Mode: %d, Enabled: %s\n", 
           state.speed, state.mode, state.enabled ? "Yes" : "No");
}
```

## Adding New Motor Drivers

To add support for a new motor driver (e.g., L298N):

1. **Create header file**: `motor_l298n.h`
2. **Create implementation**: `motor_l298n.c`
3. **Implement driver interface**:
   ```c
   const motor_driver_interface_t* l298n_get_interface(void);
   ```
4. **Add driver type**: Add `MOTOR_DRIVER_L298N` to `motor_driver_type_t`
5. **Update driver selection**: Modify `MOTOR_ACTIVE_DRIVER` in `motor_control.h`

## Pin Usage Summary

| Component | GPIO | Function |
|-----------|------|----------|
| Motor IN1 | GPIO16 | PWM control (DRV8833) |
| Motor IN2 | GPIO17 | PWM control (DRV8833) |
| Servo | GPIO4 | PWM control |
| LED Light | GPIO2 | Digital output |
| LED Horn | GPIO14 | Digital output |
| Battery Monitor | GPIO34 | ADC input |

## Troubleshooting

### Motor Not Responding

1. Check `ENABLE_MOTOR_CONTROL` is set to 1 in `project_config.h`
2. Verify GPIO pins are not conflicting with other peripherals
3. Check motor driver initialization in serial output
4. Verify motor driver power supply

### Compilation Errors

1. Ensure all header files are included properly
2. Check driver selection configuration
3. Verify ESP-IDF version compatibility

### Performance Issues

1. Adjust PWM frequency in driver configuration
2. Check for GPIO conflicts
3. Monitor system load and memory usage

## Future Enhancements

- **Multiple Motor Support**: Extend HAL for multiple motors
- **Encoder Feedback**: Add position/speed feedback
- **PID Control**: Implement closed-loop control
- **Safety Features**: Current limiting, thermal protection
- **Additional Drivers**: L298N, TB6612FNG, etc.