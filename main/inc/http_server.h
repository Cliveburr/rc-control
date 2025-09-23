#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <inttypes.h>
#include <stdbool.h>

void http_server_start(void);
void http_server_stop(void);
bool http_server_is_running(void);

// WebSocket server handle access
void* http_server_get_handle(void);

// WebSocket broadcast function
esp_err_t http_server_broadcast_ws(const char *message);

// WebSocket command handling
void process_speed_command(int speed_value);
void process_wheels_command(int wheels_value);
void process_horn_command(int horn_value);
void process_light_command(int light_value);

#endif