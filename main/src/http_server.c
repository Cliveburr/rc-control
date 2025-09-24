#include <sys/unistd.h>
#include <sys/socket.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <soc/soc.h>
#include <soc/rtc.h>
#include <esp_partition.h>

#include "project_config.h"
#include "http_server.h"
#include "ota.h"
#include "rcp_protocol.h"

#if ENABLE_LED_CONTROL
#include "led_control.h"
#endif

#if ENABLE_SERVO_CONTROL
#include "servo_control.h"
#endif

#if ENABLE_BATTERY_MONITORING
#include "battery_monitor.h"
#endif

#if ENABLE_MOTOR_CONTROL
#include "motor_control.h"
#endif

#if ENABLE_CAMERA_SUPPORT
    #include "cam.h"
    #include "esp_camera.h"
    
    #define PART_BOUNDARY "123456789000000000000987654321"
    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
    static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
#endif

static const char *TAG = "http_server";
static httpd_handle_t server = NULL;

// WebSocket client management
#define MAX_WS_CLIENTS 5

static int ws_client_fds[MAX_WS_CLIENTS];
static int ws_client_count = 0;

// Function to add WebSocket client
static void add_ws_client(int fd) {
    if (ws_client_count < MAX_WS_CLIENTS) {
        ws_client_fds[ws_client_count] = fd;
        ws_client_count++;
        ESP_LOGI(TAG, "WebSocket client fd=%d added, total clients: %d", fd, ws_client_count);
    } else {
        ESP_LOGW(TAG, "Cannot add WebSocket client fd=%d - max clients reached", fd);
    }
}

// Function to remove WebSocket client
static void remove_ws_client(int fd) {
    for (int i = 0; i < ws_client_count; i++) {
        if (ws_client_fds[i] == fd) {
            // Shift remaining clients
            for (int j = i; j < ws_client_count - 1; j++) {
                ws_client_fds[j] = ws_client_fds[j + 1];
            }
            ws_client_count--;
            ESP_LOGI(TAG, "WebSocket client fd=%d removed, total clients: %d", fd, ws_client_count);
            break;
        }
    }
}



// Function to broadcast message to all WebSocket clients
esp_err_t http_server_broadcast_ws(const char *message) {
    if (server == NULL || message == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)message;
    ws_pkt.len = strlen(message);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    int sent_count = 0;
    int removed_count = 0;
    
    // Iterate backwards to safely remove clients during iteration
    for (int i = ws_client_count - 1; i >= 0; i--) {
        esp_err_t ret = httpd_ws_send_frame_async(server, ws_client_fds[i], &ws_pkt);
        if (ret == ESP_OK) {
            sent_count++;
        } else {
            ESP_LOGW(TAG, "Failed to send to client %d: %s - removing client", 
                     ws_client_fds[i], esp_err_to_name(ret));
            
            // Remove disconnected client from list
            remove_ws_client(ws_client_fds[i]);
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        ESP_LOGI(TAG, "Removed %d disconnected WebSocket clients", removed_count);
    }
    
    ESP_LOGD(TAG, "Broadcast sent to %d/%d clients", sent_count, ws_client_count);
    return ESP_OK;
}

// Function to broadcast binary message to all WebSocket clients
esp_err_t http_server_broadcast_ws_binary(const void *data, size_t len) {
    if (server == NULL || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)data;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    int sent_count = 0;
    int removed_count = 0;
    
    // Iterate backwards to safely remove clients during iteration
    for (int i = ws_client_count - 1; i >= 0; i--) {
        esp_err_t ret = httpd_ws_send_frame_async(server, ws_client_fds[i], &ws_pkt);
        if (ret == ESP_OK) {
            sent_count++;
        } else {
            ESP_LOGW(TAG, "Failed to send binary to client %d: %s - removing client", 
                     ws_client_fds[i], esp_err_to_name(ret));
            
            // Remove disconnected client from list
            remove_ws_client(ws_client_fds[i]);
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        ESP_LOGI(TAG, "Removed %d disconnected WebSocket clients", removed_count);
    }
    
    ESP_LOGD(TAG, "Binary broadcast sent to %d/%d clients (%zu bytes)", sent_count, ws_client_count, len);
    return ESP_OK;
}

// Function to manually cleanup invalid WebSocket clients
void http_server_cleanup_ws_clients(void) {
    // Simple cleanup - no advanced validation needed
    ESP_LOGD(TAG, "WebSocket client cleanup - %d clients active", ws_client_count);
}

// Function to get current WebSocket client count
int http_server_get_ws_client_count(void) {
    return ws_client_count;
}





// WebSocket handler for receiving commands
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake done, new connection opened");
        
        // Add client to list
        add_ws_client(httpd_req_to_sockfd(req));
        
#if ENABLE_BATTERY_MONITORING
        // Send battery initialization message
        battery_send_init_message(req);
#endif
        
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    // Get frame length first
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        int client_fd = httpd_req_to_sockfd(req);
        
        // Handle different types of WebSocket errors
        switch (ret) {
            case ESP_ERR_INVALID_ARG:
            case 259: // ESP_ERR_INVALID_ARG specific value for masking
                ESP_LOGW(TAG, "WebSocket frame masking error %d (client fd=%d) - removing problematic client", ret, client_fd);
                remove_ws_client(client_fd);
                return ESP_FAIL; // Close connection immediately for masking errors
                
            case ESP_ERR_TIMEOUT:      
            case ESP_FAIL: // Generic failure (-1)
                ESP_LOGD(TAG, "WebSocket receive timeout/failure %d (client fd=%d), ignoring frame", ret, client_fd);
                return ESP_OK; // Keep connection for temporary issues            default:
                ESP_LOGW(TAG, "WebSocket receive error %d (client fd=%d), removing client", ret, client_fd);
                remove_ws_client(client_fd);
                return ESP_FAIL; // Close connection for unknown errors
        }
    }
    
    // Handle special frame types that don't have payload
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        ESP_LOGD(TAG, "Received pong from client fd=%d", httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket connection closed by client");
        remove_ws_client(httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    
    if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        ESP_LOGD(TAG, "Received ping from client fd=%d", httpd_req_to_sockfd(req));
        return ESP_OK;
    }
    
    // Process frames with payload
    if (ws_pkt.len > 0) {
        // Validate frame length to prevent buffer overflow
        if (ws_pkt.len > 1024) {
            ESP_LOGW(TAG, "WebSocket frame too large (%d bytes), ignoring", ws_pkt.len);
            return ESP_OK;
        }
        
        // Allocate buffer for payload
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for WebSocket frame (%d bytes)", ws_pkt.len);
            return ESP_ERR_NO_MEM;
        }
        
        // Receive the actual payload
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            int client_fd = httpd_req_to_sockfd(req);
            
            // Handle payload receive errors
            switch (ret) {
                case ESP_ERR_INVALID_ARG:
                case 259: // Specific masking error
                    ESP_LOGW(TAG, "WebSocket payload masking error %d (client fd=%d) - removing client", ret, client_fd);
                    remove_ws_client(client_fd);
                    free(buf);
                    return ESP_FAIL; // Close connection immediately
                    
                case ESP_ERR_TIMEOUT:
                case ESP_FAIL: // Generic receive failure (-1)
                    ESP_LOGD(TAG, "WebSocket payload timeout/failure %d (client fd=%d), ignoring frame", ret, client_fd);
                    break;
                    
                default:
                    ESP_LOGW(TAG, "WebSocket payload error %d (client fd=%d), removing client", ret, client_fd);
                    remove_ws_client(client_fd);
                    break;
            }
            
            free(buf);
            return ESP_OK;
        }
        
        // Process frame based on type
        if (ws_pkt.type == HTTPD_WS_TYPE_BINARY) {
            ESP_LOGD(TAG, "Received binary WebSocket frame (%d bytes) - processing as RCP", ws_pkt.len);

            if (ws_pkt.len < RCP_HEADER_SIZE) {
                ESP_LOGW(TAG, "RCP: Frame too small (%d bytes)", ws_pkt.len);
            } else {
                uint16_t declared_len = (uint16_t)ws_pkt.payload[0] | ((uint16_t)ws_pkt.payload[1] << 8);
                uint8_t port = ws_pkt.payload[2];
                size_t available = ws_pkt.len - RCP_HEADER_SIZE;
                size_t body_len = declared_len;

                if (body_len > available) {
                    ESP_LOGW(TAG, "RCP: Declared body length %u exceeds available %zu, truncating", declared_len, available);
                    body_len = available;
                }

                const uint8_t* body = ws_pkt.payload + RCP_HEADER_SIZE;

                esp_err_t rcp_ret = rcp_process_message(port, body, body_len);
                if (rcp_ret != ESP_OK) {
                    ESP_LOGW(TAG, "RCP: Failed to process message port=0x%02X: %s (decl_len=%u, body_len=%zu, client_fd=%d)",
                             port, esp_err_to_name(rcp_ret), declared_len, body_len, httpd_req_to_sockfd(req));
                }
            }
        } else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            // Log and reject text frames (RCP only supports binary)
            ESP_LOGW(TAG, "Received text WebSocket frame (client fd=%d) - RCP only supports binary frames", 
                     httpd_req_to_sockfd(req));
        } else if (ws_pkt.type == 5) {
            // Frame type 5 is often a continuation frame or invalid frame
            ESP_LOGW(TAG, "Received invalid/continuation WebSocket frame type 5 (client fd=%d) - removing client", 
                     httpd_req_to_sockfd(req));
            remove_ws_client(httpd_req_to_sockfd(req));
            free(buf);
            return ESP_FAIL;
        } else {
            // Log unknown frame types and potentially remove problematic clients
            ESP_LOGW(TAG, "Received unknown WebSocket frame type %d (client fd=%d) - removing client", 
                     ws_pkt.type, httpd_req_to_sockfd(req));
            remove_ws_client(httpd_req_to_sockfd(req));
            free(buf);
            return ESP_FAIL;
        }
        
        free(buf);
    }
    
    return ESP_OK;
}

void process_speed_command(int speed_value)
{
    ESP_LOGI(TAG, "Processing speed command: %d", speed_value);
#if ENABLE_MOTOR_CONTROL
    // Use the motor control HAL to set speed
    // speed_value: -100 to +100 (-100 = full reverse, 0 = stop, +100 = full forward)
    esp_err_t ret = motor_control_set_speed(speed_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set motor speed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Motor speed set to: %d", speed_value);
    }
#else
    ESP_LOGW(TAG, "Motor control disabled in project_config.h");
#endif
}

void process_wheels_command(int wheels_value)
{
    ESP_LOGI(TAG, "Processing wheels command: %d", wheels_value);
#if ENABLE_SERVO_CONTROL
    // Control servo position based on wheels command
    // wheels_value: -100 to +100 (-100 = full left, 0 = center, +100 = full right)
    esp_err_t ret = servo_control_set_position(wheels_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set servo position: %s", esp_err_to_name(ret));
    }
#else
    ESP_LOGW(TAG, "Servo control disabled in project_config.h");
#endif
}

void process_horn_command(int horn_value)
{
    ESP_LOGI(TAG, "Processing horn command: %d", horn_value);
#if ENABLE_LED_CONTROL
    // Control horn LED based on command value
    // horn_value: 1 = horn ON, 0 = horn OFF
    led_horn_set(horn_value != 0);
#else
    ESP_LOGW(TAG, "LED control disabled in project_config.h");
#endif
}

void process_light_command(int light_value)
{
    ESP_LOGI(TAG, "Processing light command: %d", light_value);
#if ENABLE_LED_CONTROL
    // Control light LED based on command value
    // light_value: 1 = light ON, 0 = light OFF
    led_light_set(light_value != 0);
#else
    ESP_LOGW(TAG, "LED control disabled in project_config.h");
#endif
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
    
    // Create binary system info response
    // Structure: [chip_model][revision][cores][cpu_freq][features][flash_size][heap_total][heap_used][heap_free][ws_clients]
    uint8_t response[32];
    int index = 0;
    
    // Chip model (1 byte): 0=Unknown, 1=ESP32, 2=ESP32-S2, 3=ESP32-S3, 4=ESP32-C3
    uint8_t chip_model_id = 0;
    switch(chip_info.model) {
        case CHIP_ESP32: chip_model_id = 1; break;
        case CHIP_ESP32S2: chip_model_id = 2; break;
        case CHIP_ESP32S3: chip_model_id = 3; break;
        case CHIP_ESP32C3: chip_model_id = 4; break;
        default: chip_model_id = 0; break;
    }
    response[index++] = chip_model_id;
    
    // Chip revision (1 byte)
    response[index++] = (uint8_t)chip_info.revision;
    
    // CPU cores (1 byte)
    response[index++] = (uint8_t)chip_info.cores;
    
    // CPU frequency in MHz (2 bytes, little endian)
    response[index++] = cpu_freq & 0xFF;
    response[index++] = (cpu_freq >> 8) & 0xFF;
    
    // Features (1 byte): bit 0=WiFi, bit 1=Bluetooth, bit 2=BLE
    uint8_t features = 0;
    if (chip_info.features & CHIP_FEATURE_WIFI_BGN) features |= 0x01;
    if (chip_info.features & CHIP_FEATURE_BT) features |= 0x02;
    if (chip_info.features & CHIP_FEATURE_BLE) features |= 0x04;
    response[index++] = features;
    
    // Flash size in MB (4 bytes, little endian)
    uint32_t flash_mb = flash_size / (1024 * 1024);
    response[index++] = flash_mb & 0xFF;
    response[index++] = (flash_mb >> 8) & 0xFF;
    response[index++] = (flash_mb >> 16) & 0xFF;
    response[index++] = (flash_mb >> 24) & 0xFF;
    
    // Heap total in KB (4 bytes, little endian)
    uint32_t heap_total_kb = total_heap / 1024;
    response[index++] = heap_total_kb & 0xFF;
    response[index++] = (heap_total_kb >> 8) & 0xFF;
    response[index++] = (heap_total_kb >> 16) & 0xFF;
    response[index++] = (heap_total_kb >> 24) & 0xFF;
    
    // Heap used in KB (4 bytes, little endian)
    uint32_t heap_used_kb = used_heap / 1024;
    response[index++] = heap_used_kb & 0xFF;
    response[index++] = (heap_used_kb >> 8) & 0xFF;
    response[index++] = (heap_used_kb >> 16) & 0xFF;
    response[index++] = (heap_used_kb >> 24) & 0xFF;
    
    // Heap free in KB (4 bytes, little endian)
    uint32_t heap_free_kb = free_heap / 1024;
    response[index++] = heap_free_kb & 0xFF;
    response[index++] = (heap_free_kb >> 8) & 0xFF;
    response[index++] = (heap_free_kb >> 16) & 0xFF;
    response[index++] = (heap_free_kb >> 24) & 0xFF;
    
    // WebSocket clients count (1 byte)
    response[index++] = (uint8_t)http_server_get_ws_client_count();
    
    // Heap usage percentage (1 byte)
    uint8_t heap_usage = (used_heap * 100) / total_heap;
    response[index++] = heap_usage;
    
    // Reserved bytes for future use (fill remaining with 0)
    while (index < sizeof(response)) {
        response[index++] = 0;
    }
    
    // Send binary response
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Content-Length", "32");
    
    ESP_LOGI(TAG, "Sending binary system info: chip=%d, rev=%d, cores=%d, freq=%dMHz, features=0x%02X, flash=%dMB, heap=%d/%dKB (%d%%), clients=%d", 
             chip_model_id, chip_info.revision, chip_info.cores, cpu_freq, features, flash_mb, 
             heap_used_kb, heap_total_kb, heap_usage, http_server_get_ws_client_count());
             
    return httpd_resp_send(req, (const char *)response, sizeof(response));
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

#if ENABLE_CAMERA_SUPPORT
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
#endif // ENABLE_CAMERA_SUPPORT

/******************************* PUBLIC METHODS *************************************/

void http_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "HTTP server is already running");
        return;
    }

#if ENABLE_CAMERA_SUPPORT
    cam_start_camera();
#endif
    
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

#if ENABLE_CAMERA_SUPPORT
    // Video streaming handler
    httpd_uri_t httpd_video = {
        .uri       = "/video",
        .method    = HTTP_GET,
        .handler   = jpg_stream_httpd_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &httpd_video);
#endif

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
    
    // Start WebSocket heartbeat timer

    
    // Initialize RCP protocol
    esp_err_t rcp_ret = rcp_init();
    if (rcp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RCP protocol: %s", esp_err_to_name(rcp_ret));
    }
    
    ESP_LOGI(TAG, "HTTP server started successfully");
}

void http_server_stop(void)
{
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        
        // Stop WebSocket heartbeat timer

        
        // Deinitialize RCP protocol
        rcp_deinit();
        
        // Clear all WebSocket clients
        ws_client_count = 0;
        
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

void* http_server_get_handle(void)
{
    return server;
}