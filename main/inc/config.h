#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <inttypes.h>

typedef enum {
   config_net_mode_none = 0,
   config_net_mode_softap = 1,
   config_net_mode_station = 2,
   config_net_mode_bluetooth = 3
} config_net_mode_t;

typedef struct {
   uint8_t version;
   config_net_mode_t net_mode;
   char softap_ssid[33];
   char softap_password[10];
   uint8_t softap_channel;
   char station_ssid[33];
   char station_password[10];
} config_data_t;

void config_init(void);
config_data_t config_load(void);
void config_save(config_data_t config_data);

#endif