#include <sys/unistd.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <cJSON.h>

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

#include "http_server.h"
//#include "cam.h"
#include "ota.h"
#include "esp_camera.h"

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