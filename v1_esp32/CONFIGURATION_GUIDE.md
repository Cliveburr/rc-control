# RC Control Project Configuration Guide

## Overview
This project uses compile-time feature flags to enable/disable hardware and software features based on your specific board configuration.

## Quick Configuration

### For Boards WITHOUT Camera (Current Setup)
No changes needed. The default configuration in `project_config.h` is:
```c
#define ENABLE_CAMERA_SUPPORT       0   // Disabled
#define ENABLE_SERVO_CONTROL        1   // Enabled
#define ENABLE_LED_CONTROL          1   // Enabled
#define ENABLE_MOTOR_CONTROL        1   // Enabled
```

### For Boards WITH Camera (ESP32-CAM)
1. Edit `main/inc/project_config.h`:
```c
#define ENABLE_CAMERA_SUPPORT       1   // Enable camera
```

2. Uncomment camera dependency in `main/idf_component.yml`:
```yaml
dependencies:
  espressif/esp32-camera: "*"   # Uncomment this line
  idf: ">=4.1.0"
```

3. Ensure PSRAM is enabled in sdkconfig:
```bash
idf.py menuconfig
# Component config → ESP PSRAM → Enable support for external PSRAM
```

## Available Features

| Feature | Config Flag | Description |
|---------|-------------|-------------|
| Camera Streaming | `ENABLE_CAMERA_SUPPORT` | Video stream via `/video` endpoint |
| Servo Control | `ENABLE_SERVO_CONTROL` | PWM servo control for steering |
| LED Control | `ENABLE_LED_CONTROL` | Horn/Light LED indicators |
| Motor Control | `ENABLE_MOTOR_CONTROL` | Motor speed control (future) |
| OTA Updates | `ENABLE_OTA_UPDATES` | Firmware update via web |
| WiFi Config | `ENABLE_WIFI_CONFIG` | WiFi setup via web interface |
| Debug Logging | `ENABLE_DEBUG_LOGGING` | Verbose debug output |

## Benefits of This Approach

### ✅ Advantages:
- **Clean compilation**: Only needed code is compiled
- **Smaller binary**: Unused features don't consume flash
- **Better performance**: No runtime checks for disabled features
- **Easy configuration**: Single header file controls all features
- **Board-specific builds**: Same codebase works on different hardware
- **Dependency management**: Only required libraries are included

### 🔧 Configuration Steps:

1. **Edit Features**: Modify `main/inc/project_config.h`
2. **Update Dependencies**: Edit `main/idf_component.yml` if camera is enabled
3. **Clean Build**: `idf.py fullclean`
4. **Rebuild**: `idf.py build`
5. **Flash**: `idf.py flash`

## Validation

The system automatically validates configurations:
- Warns if camera enabled without PSRAM
- Suggests enabling related features
- Sets appropriate log levels

## Example Configurations

### Minimal RC Car (No Camera)
```c
#define ENABLE_CAMERA_SUPPORT       0
#define ENABLE_SERVO_CONTROL        1
#define ENABLE_LED_CONTROL          1
#define ENABLE_MOTOR_CONTROL        1
#define ENABLE_OTA_UPDATES          1
#define ENABLE_DEBUG_LOGGING        0
```

### Full-Featured ESP32-CAM Car
```c
#define ENABLE_CAMERA_SUPPORT       1
#define ENABLE_SERVO_CONTROL        1
#define ENABLE_LED_CONTROL          1
#define ENABLE_MOTOR_CONTROL        1
#define ENABLE_OTA_UPDATES          1
#define ENABLE_DEBUG_LOGGING        1
```

### Testing/Development Setup
```c
#define ENABLE_CAMERA_SUPPORT       0
#define ENABLE_SERVO_CONTROL        1
#define ENABLE_LED_CONTROL          1
#define ENABLE_MOTOR_CONTROL        0  // Disabled for testing
#define ENABLE_OTA_UPDATES          1
#define ENABLE_DEBUG_LOGGING        1
```

## Runtime Behavior

When features are disabled:
- **Camera**: `/video` endpoint not registered, no streaming code compiled
- **Servo**: Commands logged but no PWM generated
- **LEDs**: Commands logged but no GPIO control
- **Motor**: Commands logged but no motor control
- **OTA**: No OTA handlers registered

This provides graceful degradation and clear feedback about what's enabled.