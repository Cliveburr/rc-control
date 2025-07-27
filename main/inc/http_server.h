#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <inttypes.h>
#include <stdbool.h>

void http_server_start(void);
void http_server_stop(void);
bool http_server_is_running(void);

// WebSocket command handling
void process_speed_command(int speed_value);
void process_horn_command(int horn_value);
void process_light_command(int light_value);

#endif