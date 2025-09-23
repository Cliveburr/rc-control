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

#include "project_config.h"
#include "http_server.h"
#include "ota.h"

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
#define WS_HEARTBEAT_INTERVAL_MS 10000  // 10 seconds
#define WS_CLIENT_TIMEOUT_MS 30000      // 30 seconds

typedef struct {
    int fd;
    int64_t last_ping_time;
    int64_t last_pong_time;
    bool waiting_for_pong;
} ws_client_info_t;

static ws_client_info_t ws_clients[MAX_WS_CLIENTS];
static int ws_client_count = 0;

// Timer for WebSocket heartbeat
static esp_timer_handle_t ws_heartbeat_timer = NULL;

// Function to add WebSocket client
static void add_ws_client(int fd) {
    if (ws_client_count < MAX_WS_CLIENTS) {
        ws_clients[ws_client_count].fd = fd;
        ws_clients[ws_client_count].last_ping_time = esp_timer_get_time() / 1000;
        ws_clients[ws_client_count].last_pong_time = esp_timer_get_time() / 1000;
        ws_clients[ws_client_count].waiting_for_pong = false;
        ws_client_count++;
        ESP_LOGI(TAG, "WebSocket client added, total clients: %d", ws_client_count);
    }
}

// Function to remove WebSocket client
static void remove_ws_client(int fd) {
    for (int i = 0; i < ws_client_count; i++) {
        if (ws_clients[i].fd == fd) {
            // Shift remaining clients
            for (int j = i; j < ws_client_count - 1; j++) {
                ws_clients[j] = ws_clients[j + 1];
            }
            ws_client_count--;
            ESP_LOGI(TAG, "WebSocket client fd=%d removed, total clients: %d", fd, ws_client_count);
            break;
        }
    }
}

// Function to find WebSocket client info
static ws_client_info_t* find_ws_client(int fd) {
    for (int i = 0; i < ws_client_count; i++) {
        if (ws_clients[i].fd == fd) {
            return &ws_clients[i];
        }
    }
    return NULL;
}

// Function to check if WebSocket client is still valid
static bool is_ws_client_valid(int fd) {
    if (fd <= 0) {
        return false;
    }
    
    // Try to send a ping frame to check if connection is alive
    httpd_ws_frame_t ping_frame = {
        .type = HTTPD_WS_TYPE_PING,
        .payload = NULL,
        .len = 0
    };
    
    esp_err_t ret = httpd_ws_send_frame_async(server, fd, &ping_frame);
    return (ret == ESP_OK);
}

// Function to clean up invalid WebSocket clients
static void cleanup_invalid_ws_clients(void) {
    int removed_count = 0;
    
    // Iterate backwards to safely remove clients during iteration
    for (int i = ws_client_count - 1; i >= 0; i--) {
        if (!is_ws_client_valid(ws_clients[i].fd)) {
            ESP_LOGW(TAG, "Removing invalid WebSocket client fd=%d", ws_clients[i].fd);
            remove_ws_client(ws_clients[i].fd);
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        ESP_LOGI(TAG, "Cleanup removed %d invalid WebSocket clients", removed_count);
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
        esp_err_t ret = httpd_ws_send_frame_async(server, ws_clients[i].fd, &ws_pkt);
        if (ret == ESP_OK) {
            sent_count++;
        } else {
            ESP_LOGW(TAG, "Failed to send to client %d: %s - removing client", 
                     ws_clients[i].fd, esp_err_to_name(ret));
            
            // Remove disconnected client from list
            remove_ws_client(ws_clients[i].fd);
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        ESP_LOGI(TAG, "Removed %d disconnected WebSocket clients", removed_count);
    }
    
    ESP_LOGD(TAG, "Broadcast sent to %d/%d clients", sent_count, ws_client_count);
    return ESP_OK;
}

// Function to manually cleanup invalid WebSocket clients
void http_server_cleanup_ws_clients(void) {
    if (server == NULL) {
        return;
    }
    cleanup_invalid_ws_clients();
}

// Function to get current WebSocket client count
int http_server_get_ws_client_count(void) {
    return ws_client_count;
}

// WebSocket heartbeat functions
static void send_ping_to_client(int fd) {
    if (server && is_ws_client_valid(fd)) {
        httpd_ws_frame_t ping_frame = {
            .type = HTTPD_WS_TYPE_PING,
            .payload = NULL,
            .len = 0
        };
        
        esp_err_t ret = httpd_ws_send_frame_async(server, fd, &ping_frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send ping to client fd=%d: %s", fd, esp_err_to_name(ret));
            remove_ws_client(fd);
        } else {
            ws_client_info_t* client = find_ws_client(fd);
            if (client) {
                client->last_ping_time = esp_timer_get_time() / 1000;
                client->waiting_for_pong = true;
            }
        }
    }
}

static void check_client_timeouts(void) {
    int64_t current_time = esp_timer_get_time() / 1000;
    
    for (int i = ws_client_count - 1; i >= 0; i--) {
        ws_client_info_t* client = &ws_clients[i];
        
        // Check if client hasn't responded to ping
        if (client->waiting_for_pong && 
            (current_time - client->last_ping_time) > WS_CLIENT_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Client fd=%d timeout (no pong received), removing", client->fd);
            remove_ws_client(client->fd);
        }
    }
}

static void ws_heartbeat_timer_callback(void* arg) {
    // Send ping to all clients
    for (int i = 0; i < ws_client_count; i++) {
        send_ping_to_client(ws_clients[i].fd);
    }
    
    // Check for timeouts
    check_client_timeouts();
    
    // Cleanup any invalid clients
    cleanup_invalid_ws_clients();
}

static void start_ws_heartbeat_timer(void) {
    if (ws_heartbeat_timer != NULL) {
        return; // Timer already started
    }
    
    esp_timer_create_args_t timer_config = {
        .callback = ws_heartbeat_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ws_heartbeat"
    };
    
    esp_err_t ret = esp_timer_create(&timer_config, &ws_heartbeat_timer);
    if (ret == ESP_OK) {
        ret = esp_timer_start_periodic(ws_heartbeat_timer, WS_HEARTBEAT_INTERVAL_MS * 1000);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WebSocket heartbeat timer started (interval: %d ms)", WS_HEARTBEAT_INTERVAL_MS);
        } else {
            ESP_LOGE(TAG, "Failed to start heartbeat timer: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Failed to create heartbeat timer: %s", esp_err_to_name(ret));
    }
}

static void stop_ws_heartbeat_timer(void) {
    if (ws_heartbeat_timer != NULL) {
        esp_timer_stop(ws_heartbeat_timer);
        esp_timer_delete(ws_heartbeat_timer);
        ws_heartbeat_timer = NULL;
        ESP_LOGI(TAG, "WebSocket heartbeat timer stopped");
    }
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
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // Handle frame errors gracefully instead of closing connection
        if (ret == ESP_ERR_INVALID_ARG) {
            ESP_LOGW(TAG, "WebSocket frame masking error (client fd=%d), ignoring frame", httpd_req_to_sockfd(req));
        } else {
            ESP_LOGW(TAG, "httpd_ws_recv_frame failed to get frame len with %d (client fd=%d)", ret, httpd_req_to_sockfd(req));
        }
        
        // Don't close connection for frame errors - just ignore the bad frame
        return ESP_OK;
    }
    
    if (ws_pkt.len) {
        // Validate frame length to prevent buffer overflow
        if (ws_pkt.len > 1024) {
            ESP_LOGW(TAG, "WebSocket frame too large (%d bytes), ignoring", ws_pkt.len);
            return ESP_OK;
        }
        
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "httpd_ws_recv_frame failed with %d (client fd=%d), ignoring frame", ret, httpd_req_to_sockfd(req));
            free(buf);
            // Don't close connection - just ignore the bad frame
            return ESP_OK;
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
                    //ESP_LOGI(TAG, "Received speed command: %d", speed_value);
                    process_speed_command(speed_value);
                } else if (strcmp(command_type, "wheels") == 0) {
                    int wheels_value = (int)value->valuedouble;
                    //ESP_LOGI(TAG, "Received wheels command: %d", wheels_value);
                    process_wheels_command(wheels_value);
                } else if (strcmp(command_type, "horn") == 0) {
                    int horn_value = (int)value->valuedouble;
                    //ESP_LOGI(TAG, "Received horn command: %d", horn_value);
                    process_horn_command(horn_value);
                } else if (strcmp(command_type, "light") == 0) {
                    int light_value = (int)value->valuedouble;
                    //ESP_LOGI(TAG, "Received light command: %d", light_value);
                    process_light_command(light_value);
                }
            } else {
                ESP_LOGW(TAG, "Invalid WebSocket command format (client fd=%d)", httpd_req_to_sockfd(req));
            }
            
            cJSON_Delete(json);
        } else {
            ESP_LOGW(TAG, "Failed to parse JSON: %s (client fd=%d)", (char*)ws_pkt.payload, httpd_req_to_sockfd(req));
        }
        
        free(buf);
    }
    
    // Handle PONG frames for heartbeat
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        int client_fd = httpd_req_to_sockfd(req);
        ws_client_info_t* client = find_ws_client(client_fd);
        if (client) {
            client->last_pong_time = esp_timer_get_time() / 1000;
            client->waiting_for_pong = false;
            ESP_LOGD(TAG, "Received pong from client fd=%d", client_fd);
        }
    }
    
    // Check if the connection is being closed
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket connection closed by client");
        remove_ws_client(httpd_req_to_sockfd(req));
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
    
#if ENABLE_MOTOR_CONTROL
    // Motor status information
    cJSON *motor = cJSON_CreateObject();
    cJSON_AddBoolToObject(motor, "enabled", true);
    
    motor_state_t motor_state;
    esp_err_t motor_ret = motor_control_get_state(&motor_state);
    if (motor_ret == ESP_OK) {
        cJSON_AddNumberToObject(motor, "speed", motor_state.speed);
        cJSON_AddNumberToObject(motor, "mode", motor_state.mode);
        cJSON_AddBoolToObject(motor, "active", motor_state.enabled);
        
        const char* mode_str;
        switch (motor_state.mode) {
            case MOTOR_MODE_FORWARD: mode_str = "forward"; break;
            case MOTOR_MODE_REVERSE: mode_str = "reverse"; break;
            case MOTOR_MODE_BRAKE: mode_str = "brake"; break;
            case MOTOR_MODE_FREE: mode_str = "free"; break;
            default: mode_str = "unknown"; break;
        }
        cJSON_AddStringToObject(motor, "mode_str", mode_str);
    } else {
        cJSON_AddNumberToObject(motor, "speed", 0);
        cJSON_AddNumberToObject(motor, "mode", MOTOR_MODE_FREE);
        cJSON_AddBoolToObject(motor, "active", false);
        cJSON_AddStringToObject(motor, "mode_str", "error");
    }
    
    cJSON_AddItemToObject(json, "motor", motor);
#else
    // Motor disabled
    cJSON *motor = cJSON_CreateObject();
    cJSON_AddBoolToObject(motor, "enabled", false);
    cJSON_AddStringToObject(motor, "status", "disabled");
    cJSON_AddItemToObject(json, "motor", motor);
#endif
    
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
    start_ws_heartbeat_timer();
    
    ESP_LOGI(TAG, "HTTP server started successfully");
}

void http_server_stop(void)
{
    if (server != NULL) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        
        // Stop WebSocket heartbeat timer
        stop_ws_heartbeat_timer();
        
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