#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <inttypes.h>
#include <stdbool.h>

void http_server_start(void);
void http_server_stop(void);
bool http_server_is_running(void);

#endif