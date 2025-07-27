#ifndef __LED_CONTROL_H__
#define __LED_CONTROL_H__

#include <inttypes.h>
#include <stdbool.h>

// GPIO pins for LEDs (using available GPIO pins)
#define LED_LIGHT_PIN   2   // GPIO2 for light LED
#define LED_HORN_PIN    14  // GPIO14 for horn LED

// LED control functions
void led_control_init(void);
void led_light_set(bool state);
void led_horn_set(bool state);
void led_light_toggle(void);
void led_horn_toggle(void);

#endif
