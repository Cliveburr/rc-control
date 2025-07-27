#include <sys/unistd.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <soc/soc.h>
#include <soc/rtc.h>
#include <esp_partition.h>

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

#include "http_server.h"
//#include "cam.h"
#include "ota.h"
#include "esp_camera.h"
#include "led_control.h"

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// WebSocket handler for receiving commands
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake done, new connection opened");
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    
    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        
        // Parse JSON command
        cJSON *json = cJSON_Parse((char*)ws_pkt.payload);
        if (json != NULL) {
            cJSON *type = cJSON_GetObjectItem(json, "type");
            cJSON *value = cJSON_GetObjectItem(json, "value");
            
            if (cJSON_IsString(type) && cJSON_IsNumber(value)) {
                const char *command_type = type->valuestring;
                
                if (strcmp(command_type, "speed") == 0) {
                    int speed_value = (int)value->valuedouble;
                    ESP_LOGI(TAG, "Received speed command: %d", speed_value);
                    process_speed_command(speed_value);
                } else if (strcmp(command_type, "wheels") == 0) {
                    int wheels_value = (int)value->valuedouble;
                    ESP_LOGI(TAG, "Received wheels command: %d", wheels_value);
                    process_wheels_command(wheels_value);
                } else if (strcmp(command_type, "horn") == 0) {
                    int horn_value = (int)value->valuedouble;
                    ESP_LOGI(TAG, "Received horn command: %d", horn_value);
                    process_horn_command(horn_value);
                } else if (strcmp(command_type, "light") == 0) {
                    int light_value = (int)value->valuedouble;
                    ESP_LOGI(TAG, "Received light command: %d", light_value);
                    process_light_command(light_value);
                }
            }
            
            cJSON_Delete(json);
        } else {
            ESP_LOGW(TAG, "Failed to parse JSON: %s", (char*)ws_pkt.payload);
        }
        
        free(buf);
    }
    
    return ESP_OK;
}

void process_speed_command(int speed_value)
{
    ESP_LOGI(TAG, "Processing speed command: %d", speed_value);
    // TODO: Implement actual speed control logic here
    // For now, just log the received value
}

void process_wheels_command(int wheels_value)
{
    ESP_LOGI(TAG, "Processing wheels command: %d", wheels_value);
    // TODO: Implement actual wheels/steering control logic here
    // wheels_value: -100 to +100 (-100 = full left, 0 = center, +100 = full right)
    // For now, just log the received value
}

void process_horn_command(int horn_value)
{
    ESP_LOGI(TAG, "Processing horn command: %d", horn_value);
    // Control horn LED based on command value
    // horn_value: 1 = horn ON, 0 = horn OFF
    led_horn_set(horn_value != 0);
}

void process_light_command(int light_value)
{
    ESP_LOGI(TAG, "Processing light command: %d", light_value);
    // Control light LED based on command value
    // light_value: 1 = light ON, 0 = light OFF
    led_light_set(light_value != 0);
}

static esp_err_t system_info_handler(httpd_req_t *req)
{
    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    // Get memory information
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t used_heap = total_heap - free_heap;
    
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t used_psram = total_psram - free_psram;
    
    size_t total_dma = heap_caps_get_total_size(MALLOC_CAP_DMA);
    size_t free_dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t used_dma = total_dma - free_dma;
    
    // Get IRAM information
    size_t total_iram = heap_caps_get_total_size(MALLOC_CAP_IRAM_8BIT);
    size_t free_iram = heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT);
    size_t used_iram = total_iram - free_iram;
    
    // Get DRAM information (internal 8-bit accessible)
    size_t total_dram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t free_dram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t used_dram = total_dram - free_dram;
    
    // Get CPU frequency
    rtc_cpu_freq_config_t cpu_config;
    rtc_clk_cpu_freq_get_config(&cpu_config);
    uint32_t cpu_freq = cpu_config.freq_mhz;
    
    // Get flash size from partition table
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    uint32_t flash_size = 0;
    if (partition != NULL) {
        flash_size = partition->size + partition->address; // Approximate total flash size
    }
    
    // Create JSON response
    cJSON *json = cJSON_CreateObject();
    cJSON *chip = cJSON_CreateObject();
    cJSON *memory = cJSON_CreateObject();
    cJSON *heap = cJSON_CreateObject();
    cJSON *psram = cJSON_CreateObject();
    cJSON *dma = cJSON_CreateObject();
    cJSON *iram = cJSON_CreateObject();
    cJSON *dram = cJSON_CreateObject();
    
    // Chip information
    const char* chip_model;
    switch(chip_info.model) {
        case CHIP_ESP32: chip_model = "ESP32"; break;
        case CHIP_ESP32S2: chip_model = "ESP32-S2"; break;
        case CHIP_ESP32S3: chip_model = "ESP32-S3"; break;
        case CHIP_ESP32C3: chip_model = "ESP32-C3"; break;
        default: chip_model = "Unknown"; break;
    }
    
    cJSON_AddStringToObject(chip, "model", chip_model);
    cJSON_AddNumberToObject(chip, "cores", chip_info.cores);
    cJSON_AddNumberToObject(chip, "revision", chip_info.revision);
    cJSON_AddNumberToObject(chip, "cpu_freq_mhz", cpu_freq);
    cJSON_AddBoolToObject(chip, "has_wifi", (chip_info.features & CHIP_FEATURE_WIFI_BGN) != 0);
    cJSON_AddBoolToObject(chip, "has_bluetooth", (chip_info.features & CHIP_FEATURE_BT) != 0);
    cJSON_AddBoolToObject(chip, "has_ble", (chip_info.features & CHIP_FEATURE_BLE) != 0);
    cJSON_AddNumberToObject(chip, "flash_size_mb", flash_size / (1024 * 1024));
    
    // Heap memory
    cJSON_AddNumberToObject(heap, "total_bytes", total_heap);
    cJSON_AddNumberToObject(heap, "used_bytes", used_heap);
    cJSON_AddNumberToObject(heap, "free_bytes", free_heap);
    cJSON_AddNumberToObject(heap, "usage_percent", (used_heap * 100) / total_heap);
    
    // PSRAM memory
    if (total_psram > 0) {
        cJSON_AddNumberToObject(psram, "total_bytes", total_psram);
        cJSON_AddNumberToObject(psram, "used_bytes", used_psram);
        cJSON_AddNumberToObject(psram, "free_bytes", free_psram);
        cJSON_AddNumberToObject(psram, "usage_percent", (used_psram * 100) / total_psram);
    } else {
        cJSON_AddNumberToObject(psram, "total_bytes", 0);
        cJSON_AddNumberToObject(psram, "used_bytes", 0);
        cJSON_AddNumberToObject(psram, "free_bytes", 0);
        cJSON_AddNumberToObject(psram, "usage_percent", 0);
    }
    
    // DMA memory
    cJSON_AddNumberToObject(dma, "total_bytes", total_dma);
    cJSON_AddNumberToObject(dma, "used_bytes", used_dma);
    cJSON_AddNumberToObject(dma, "free_bytes", free_dma);
    cJSON_AddNumberToObject(dma, "usage_percent", (used_dma * 100) / total_dma);
    
    // IRAM memory
    cJSON_AddNumberToObject(iram, "total_bytes", total_iram);
    cJSON_AddNumberToObject(iram, "used_bytes", used_iram);
    cJSON_AddNumberToObject(iram, "free_bytes", free_iram);
    cJSON_AddNumberToObject(iram, "usage_percent", total_iram > 0 ? (used_iram * 100) / total_iram : 0);
    
    // DRAM memory
    cJSON_AddNumberToObject(dram, "total_bytes", total_dram);
    cJSON_AddNumberToObject(dram, "used_bytes", used_dram);
    cJSON_AddNumberToObject(dram, "free_bytes", free_dram);
    cJSON_AddNumberToObject(dram, "usage_percent", total_dram > 0 ? (used_dram * 100) / total_dram : 0);
    
    // Add memory objects to memory
    cJSON_AddItemToObject(memory, "heap", heap);
    cJSON_AddItemToObject(memory, "psram", psram);
    cJSON_AddItemToObject(memory, "dma", dma);
    cJSON_AddItemToObject(memory, "iram", iram);
    cJSON_AddItemToObject(memory, "dram", dram);
    
    // Add main objects to JSON
    cJSON_AddItemToObject(json, "chip", chip);
    cJSON_AddItemToObject(json, "memory", memory);
    
    // Convert to string and send response
    char *json_string = cJSON_Print(json);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    // Clean up
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

static esp_err_t httpd_get_handler(httpd_req_t *req)
{
    extern const uint8_t index_html_start[] asm("_binary_index_html_start");
    extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);

    //httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);

    return ESP_OK;
}

esp_err_t jpg_stream_httpd_handler(httpd_req_t *req)
{
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];
    static int64_t last_frame = 0;
    if (!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true)
    {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }

        if (fb->format != PIXFORMAT_JPEG)
        {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if (!jpeg_converted)
            {
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        }
        else
        {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        if (res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }

        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if (fb->format != PIXFORMAT_JPEG)
        {
            free(_jpg_buf);
        }

        esp_camera_fb_return(fb);
        if (res != ESP_OK)
        {
            break;
        }

        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %uKB %ums (%.1ffps)",
            (unsigned int)(_jpg_buf_len/1024),
            (unsigned int)frame_time, 1000.0 / (uint32_t)frame_time);
    }

    last_frame = 0;
    return res;
}

/******************************* PUBLIC METHODS *************************************/

void http_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "HTTP server is already running");
        return;
    }

    //cam_start_camera();
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    // Register WebSocket handler for commands
    httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
    };
    httpd_register_uri_handler(server, &ws);

    httpd_uri_t httpd_video = {
        .uri       = "/video",
        .method    = HTTP_GET,
        .handler   = jpg_stream_httpd_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &httpd_video);

    httpd_uri_t ota_upload = {
        .uri       = "/ota/upload",
        .method    = HTTP_POST,
        .handler   = ota_upload_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &ota_upload);

    httpd_uri_t ota_status = {
        .uri       = "/ota/status",
        .method    = HTTP_GET,
        .handler   = ota_status_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &ota_status);

    httpd_uri_t ota_restart = {
        .uri       = "/ota/restart",
        .method    = HTTP_POST,
        .handler   = ota_restart_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &ota_restart);

    httpd_uri_t system_info = {
        .uri       = "/api/system-info",
        .method    = HTTP_GET,
        .handler   = system_info_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &system_info);

    httpd_uri_t httpd_get = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = httpd_get_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &httpd_get);
}

void http_server_stop(void)
{
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        ESP_ERROR_CHECK(httpd_stop(server));
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped successfully");
    } else {
        ESP_LOGI(TAG, "HTTP server stop requested, but server is not running");
    }
}

bool http_server_is_running(void)
{
    return server != NULL;
}