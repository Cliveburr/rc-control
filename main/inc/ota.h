#ifndef __OTA_H__
#define __OTA_H__

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Initialize OTA system
 */
void ota_init(void);

/**
 * @brief HTTP handler for OTA firmware upload
 */
esp_err_t ota_upload_handler(httpd_req_t *req);

/**
 * @brief HTTP handler for OTA status
 */
esp_err_t ota_status_handler(httpd_req_t *req);

/**
 * @brief Check if OTA update is available and perform update
 */
esp_err_t ota_perform_update(void);

/**
 * @brief Get current running partition info
 */
esp_err_t ota_get_partition_info(char *buffer, size_t buffer_size);

/**
 * @brief HTTP handler for system restart
 */
esp_err_t ota_restart_handler(httpd_req_t *req);

#endif
