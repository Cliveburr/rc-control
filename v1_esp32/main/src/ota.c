#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_flash_partitions.h>
#include <esp_partition.h>
#include <esp_http_server.h>

#include "ota.h"

static const char *TAG = "ota";

static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = NULL;
static bool ota_in_progress = false;
static size_t binary_file_length = 0;
static size_t data_read = 0;

void ota_init(void)
{
    ESP_LOGI(TAG, "OTA system initialized");
    
    // Print current partition info
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "An OTA update has been performed. Validating...");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
    
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);
}

esp_err_t ota_get_partition_info(char *buffer, size_t buffer_size)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    
    snprintf(buffer, buffer_size, 
        "{\"running_partition\":\"%s\",\"boot_partition\":\"%s\",\"ota_in_progress\":%s}",
        running->label, boot->label, ota_in_progress ? "true" : "false");
    
    return ESP_OK;
}

esp_err_t ota_status_handler(httpd_req_t *req)
{
    char response[256];
    ota_get_partition_info(response, sizeof(response));
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

esp_err_t ota_upload_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Starting OTA update...");
    
    if (ota_in_progress) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "OTA already in progress");
        return ESP_FAIL;
    }
    
    // Get content length
    size_t content_len = req->content_len;
    if (content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No content");
        return ESP_FAIL;
    }
    
    // Check if content length is reasonable (not too small, not too large)
    if (content_len < 100000) {  // Less than 100KB probably not a valid firmware
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too small to be valid firmware");
        return ESP_FAIL;
    }
    
    if (content_len > 2000000) {  // More than 2MB probably too large
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File too large");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Content length: %d", content_len);
    
    // Find update partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    
    // Begin OTA update
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }
    
    ota_in_progress = true;
    binary_file_length = content_len;
    data_read = 0;
    
    // Buffer for receiving data
    char *buffer = malloc(1024);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        esp_ota_abort(ota_handle);
        ota_in_progress = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    // Receive and write data
    while (data_read < binary_file_length) {
        size_t remaining = binary_file_length - data_read;
        size_t to_read = (remaining < 1024) ? remaining : 1024;
        int data_recv = httpd_req_recv(req, buffer, to_read);
        if (data_recv < 0) {
            if (data_recv == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGW(TAG, "Socket timeout");
                continue;
            }
            ESP_LOGE(TAG, "File reception failed");
            break;
        } else if (data_recv > 0) {
            err = esp_ota_write(ota_handle, (const void *)buffer, data_recv);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                break;
            }
            data_read += data_recv;
            ESP_LOGI(TAG, "Received %d of %d bytes", data_read, binary_file_length);
        }
    }
    
    free(buffer);
    
    if (data_read != binary_file_length) {
        ESP_LOGE(TAG, "Error in receiving file");
        esp_ota_abort(ota_handle);
        ota_in_progress = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File transfer incomplete");
        return ESP_FAIL;
    }
    
    // Finalize OTA update
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        ota_in_progress = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA validation failed");
        return ESP_FAIL;
    }
    
    // Set boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        ota_in_progress = false;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        return ESP_FAIL;
    }
    
    ota_in_progress = false;
    ESP_LOGI(TAG, "OTA update successful! Restarting in 3 seconds...");
    
    // Send success response
    const char* response = "{\"status\":\"success\",\"message\":\"OTA update completed successfully. Device will restart.\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    // Restart after a delay
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}

esp_err_t ota_perform_update(void)
{
    // This function could be used for automatic updates from a server
    // For now, we'll just return ESP_OK as manual upload is preferred
    return ESP_OK;
}

esp_err_t ota_restart_handler(httpd_req_t *req)
{
    const char* response = "{\"status\":\"success\",\"message\":\"System will restart in 3 seconds\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    ESP_LOGI(TAG, "Remote restart requested. Restarting in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}
