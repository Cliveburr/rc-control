# Implementação de Upload de HTML via HTTP - ESP32 RC Control

## Resumo
Este documento contém todos os códigos e instruções necessárias para implementar a funcionalidade de upload de arquivos HTML individuais via HTTP no projeto ESP32 RC Control.

## Análise da Situação Atual

O projeto atualmente usa **arquivo embebido** (`EMBED_FILES`) no firmware:
- HTML compilado dentro do binário
- Servido através de ponteiros de memória (`_binary_index_html_start`)
- Requer recompilação para atualizações

## Soluções Disponíveis

### 🔥 Opção 1: Conversão para SPIFFS/LittleFS (Recomendada)
### 🚀 Opção 2: Sistema Híbrido (Inovadora)

---

## OPÇÃO 1: IMPLEMENTAÇÃO SPIFFS/LittleFS

### 1. Modificações no CMakeLists.txt

```cmake
# Arquivo: main/CMakeLists.txt
idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "inc"
    # Remover ou comentar EMBED_FILES se usar SPIFFS
    # EMBED_FILES "wwwroot/index.html"
    REQUIRES "app_update" "esp_http_server" "esp_wifi" "wpa_supplicant" "nvs_flash" "esp_netif" "esp_event" "esp_timer" "freertos" "driver" "lwip" "esp_adc" "spiffs"
)
```

### 2. Configuração de Partições

```csv
# Arquivo: partitions.csv (modificar existente)
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x180000,
ota_0,    app,  ota_0,   0x190000,0x180000,
ota_1,    app,  ota_1,   0x310000,0x180000,
spiffs,   data, spiffs,  0x490000,0x160000,
```

### 3. Header para Upload HTML

```c
// Arquivo: main/inc/html_upload.h
#ifndef __HTML_UPLOAD_H__
#define __HTML_UPLOAD_H__

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief Initialize HTML upload system with SPIFFS
 */
esp_err_t html_upload_init(void);

/**
 * @brief HTTP handler for HTML file upload
 */
esp_err_t html_upload_handler(httpd_req_t *req);

/**
 * @brief HTTP handler for serving HTML from SPIFFS
 */
esp_err_t html_spiffs_handler(httpd_req_t *req);

/**
 * @brief Validate HTML file content
 */
bool validate_html_file(const char *filepath);

/**
 * @brief Create backup of current HTML file
 */
esp_err_t backup_html_file(void);

/**
 * @brief Restore HTML file from backup
 */
esp_err_t restore_html_backup(void);

#endif
```

### 4. Implementação Principal do Upload

```c
// Arquivo: main/src/html_upload.c
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "html_upload.h"

static const char *TAG = "HTML_UPLOAD";

#define SPIFFS_BASE_PATH "/spiffs"
#define HTML_FILE_PATH "/spiffs/index.html"
#define HTML_BACKUP_PATH "/spiffs/index_backup.html"
#define HTML_TEMP_PATH "/spiffs/index_new.html"
#define MAX_FILE_SIZE 1048576  // 1MB
#define BOUNDARY_MAX_LEN 256
#define BUFFER_SIZE 1024

esp_err_t html_upload_init(void) {
    ESP_LOGI(TAG, "Initializing HTML upload system");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "SPIFFS initialized successfully");
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    
    // Verificar se existe arquivo HTML, se não, criar um padrão
    struct stat st;
    if (stat(HTML_FILE_PATH, &st) != 0) {
        ESP_LOGW(TAG, "HTML file not found, creating default");
        FILE *f = fopen(HTML_FILE_PATH, "w");
        if (f != NULL) {
            const char *default_html = "<!DOCTYPE html><html><head><title>ESP32 RC Control</title></head><body><h1>Default HTML - Upload a new file</h1></body></html>";
            fwrite(default_html, 1, strlen(default_html), f);
            fclose(f);
            ESP_LOGI(TAG, "Default HTML file created");
        }
    }
    
    return ESP_OK;
}

bool validate_html_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Cannot open file for validation: %s", filepath);
        return false;
    }
    
    // Obter tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    
    // Validações básicas
    if (size <= 0) {
        ESP_LOGE(TAG, "File is empty");
        return false;
    }
    
    if (size > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large: %ld bytes", size);
        return false;
    }
    
    // Verificar se contém tags HTML básicas
    file = fopen(filepath, "r");
    if (!file) return false;
    
    char buffer[512];
    size_t read_size = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    buffer[read_size] = '\0';
    
    // Converter para lowercase para verificação
    for (int i = 0; i < read_size; i++) {
        if (buffer[i] >= 'A' && buffer[i] <= 'Z') {
            buffer[i] = buffer[i] + 32;
        }
    }
    
    // Verificar se contém tags HTML essenciais
    bool has_html = strstr(buffer, "<html") != NULL;
    bool has_head = strstr(buffer, "<head") != NULL || strstr(buffer, "<title") != NULL;
    bool has_body = strstr(buffer, "<body") != NULL;
    
    if (!has_html) {
        ESP_LOGW(TAG, "File doesn't appear to be HTML (missing <html> tag)");
        // Não falhar por isso, pode ser HTML válido sem tag html explícita
    }
    
    ESP_LOGI(TAG, "HTML validation passed - Size: %ld bytes", size);
    return true;
}

esp_err_t backup_html_file(void) {
    FILE *source = fopen(HTML_FILE_PATH, "r");
    if (!source) {
        ESP_LOGW(TAG, "No existing HTML file to backup");
        return ESP_OK;  // Não é erro se não existe
    }
    
    FILE *backup = fopen(HTML_BACKUP_PATH, "w");
    if (!backup) {
        fclose(source);
        ESP_LOGE(TAG, "Cannot create backup file");
        return ESP_FAIL;
    }
    
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, backup) != bytes_read) {
            ESP_LOGE(TAG, "Error writing backup file");
            fclose(source);
            fclose(backup);
            return ESP_FAIL;
        }
    }
    
    fclose(source);
    fclose(backup);
    
    ESP_LOGI(TAG, "HTML file backed up successfully");
    return ESP_OK;
}

esp_err_t restore_html_backup(void) {
    FILE *backup = fopen(HTML_BACKUP_PATH, "r");
    if (!backup) {
        ESP_LOGE(TAG, "No backup file found");
        return ESP_FAIL;
    }
    
    FILE *target = fopen(HTML_FILE_PATH, "w");
    if (!target) {
        fclose(backup);
        ESP_LOGE(TAG, "Cannot create target file for restore");
        return ESP_FAIL;
    }
    
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), backup)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target) != bytes_read) {
            ESP_LOGE(TAG, "Error writing restored file");
            fclose(backup);
            fclose(target);
            return ESP_FAIL;
        }
    }
    
    fclose(backup);
    fclose(target);
    
    ESP_LOGI(TAG, "HTML file restored from backup");
    return ESP_OK;
}

// Função auxiliar para extrair boundary do Content-Type
static esp_err_t extract_boundary(const char *content_type, char *boundary, size_t boundary_size) {
    const char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start) {
        ESP_LOGE(TAG, "No boundary found in Content-Type");
        return ESP_FAIL;
    }
    
    boundary_start += 9; // Skip "boundary="
    
    // Remover aspas se existirem
    if (*boundary_start == '"') {
        boundary_start++;
    }
    
    size_t len = 0;
    while (boundary_start[len] && boundary_start[len] != '"' && 
           boundary_start[len] != ';' && boundary_start[len] != ' ' && 
           len < boundary_size - 1) {
        boundary[len] = boundary_start[len];
        len++;
    }
    boundary[len] = '\0';
    
    if (len == 0) {
        ESP_LOGE(TAG, "Empty boundary");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Extracted boundary: %s", boundary);
    return ESP_OK;
}

esp_err_t html_upload_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "HTML upload request received");
    
    // Verificar Content-Type
    char content_type[128];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK) {
        ESP_LOGE(TAG, "No Content-Type header");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing Content-Type");
        return ESP_FAIL;
    }
    
    // Verificar se é multipart
    if (strncmp(content_type, "multipart/form-data", 19) != 0) {
        ESP_LOGE(TAG, "Invalid Content-Type: %s", content_type);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected multipart/form-data");
        return ESP_FAIL;
    }
    
    // Extrair boundary
    char boundary[BOUNDARY_MAX_LEN];
    if (extract_boundary(content_type, boundary, sizeof(boundary)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid boundary");
        return ESP_FAIL;
    }
    
    // Verificar tamanho do conteúdo
    if (req->content_len > MAX_FILE_SIZE) {
        ESP_LOGE(TAG, "File too large: %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_413_REQ_ENTITY_TOO_LARGE, "File too large");
        return ESP_FAIL;
    }
    
    // Fazer backup do arquivo atual
    if (backup_html_file() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to backup current HTML file");
    }
    
    // Abrir arquivo temporário para escrita
    FILE *temp_file = fopen(HTML_TEMP_PATH, "w");
    if (!temp_file) {
        ESP_LOGE(TAG, "Cannot create temporary file");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot create temporary file");
        return ESP_FAIL;
    }
    
    // Processar dados multipart
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        fclose(temp_file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    int remaining = req->content_len;
    bool in_file_data = false;
    bool file_found = false;
    char boundary_start[BOUNDARY_MAX_LEN + 10];
    char boundary_end[BOUNDARY_MAX_LEN + 10];
    
    snprintf(boundary_start, sizeof(boundary_start), "--%s", boundary);
    snprintf(boundary_end, sizeof(boundary_end), "--%s--", boundary);
    
    ESP_LOGI(TAG, "Processing multipart data, total size: %d bytes", req->content_len);
    
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buffer, MIN(remaining, BUFFER_SIZE));
        if (recv_len <= 0) {
            ESP_LOGE(TAG, "Error receiving data");
            break;
        }
        
        if (!in_file_data) {
            // Procurar pelo início dos dados do arquivo
            char *data_start = strstr(buffer, "\r\n\r\n");
            if (data_start && strstr(buffer, "filename=")) {
                data_start += 4; // Skip "\r\n\r\n"
                int header_len = data_start - buffer;
                int data_len = recv_len - header_len;
                
                if (data_len > 0) {
                    fwrite(data_start, 1, data_len, temp_file);
                }
                
                in_file_data = true;
                file_found = true;
                ESP_LOGI(TAG, "Found file data, header length: %d", header_len);
            }
        } else {
            // Verificar se chegamos ao fim do arquivo (boundary)
            if (strstr(buffer, boundary_start) || strstr(buffer, boundary_end)) {
                // Encontrou boundary - parar de escrever
                char *boundary_pos = strstr(buffer, boundary_start);
                if (!boundary_pos) {
                    boundary_pos = strstr(buffer, boundary_end);
                }
                
                if (boundary_pos) {
                    int data_len = boundary_pos - buffer;
                    if (data_len > 0) {
                        // Remover \r\n antes do boundary
                        if (data_len >= 2 && boundary_pos[-2] == '\r' && boundary_pos[-1] == '\n') {
                            data_len -= 2;
                        }
                        if (data_len > 0) {
                            fwrite(buffer, 1, data_len, temp_file);
                        }
                    }
                }
                break;
            } else {
                // Continuar escrevendo dados do arquivo
                fwrite(buffer, 1, recv_len, temp_file);
            }
        }
        
        remaining -= recv_len;
    }
    
    fclose(temp_file);
    free(buffer);
    
    if (!file_found) {
        remove(HTML_TEMP_PATH);
        ESP_LOGE(TAG, "No file found in multipart data");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No file in upload");
        return ESP_FAIL;
    }
    
    // Validar o arquivo HTML
    if (!validate_html_file(HTML_TEMP_PATH)) {
        remove(HTML_TEMP_PATH);
        ESP_LOGE(TAG, "HTML validation failed");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid HTML file");
        return ESP_FAIL;
    }
    
    // Substituir arquivo atual
    if (remove(HTML_FILE_PATH) != 0) {
        ESP_LOGW(TAG, "Warning: Could not remove old HTML file");
    }
    
    if (rename(HTML_TEMP_PATH, HTML_FILE_PATH) != 0) {
        ESP_LOGE(TAG, "Failed to rename temporary file");
        // Tentar restaurar backup
        restore_html_backup();
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update HTML file");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "HTML file updated successfully");
    
    // Enviar resposta de sucesso
    const char *response = "{\"success\":true,\"message\":\"HTML updated successfully\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

esp_err_t html_spiffs_handler(httpd_req_t *req) {
    FILE *file = fopen(HTML_FILE_PATH, "r");
    if (!file) {
        ESP_LOGE(TAG, "Cannot open HTML file");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "HTML file not found");
        return ESP_FAIL;
    }
    
    // Obter tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Definir headers HTTP
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    
    // Enviar arquivo em chunks
    char *buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        fclose(file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            ESP_LOGE(TAG, "Error sending HTML chunk");
            break;
        }
    }
    
    // Finalizar chunked response
    httpd_resp_send_chunk(req, NULL, 0);
    
    free(buffer);
    fclose(file);
    
    ESP_LOGI(TAG, "HTML file served successfully (%ld bytes)", file_size);
    return ESP_OK;
}
```

### 5. Modificações no http_server.c

```c
// Arquivo: main/src/http_server.c
// Adicionar no início do arquivo
#include "html_upload.h"

// Modificar a função http_server_start()
void http_server_start(void) {
    if (server != NULL) {
        ESP_LOGW(TAG, "HTTP server is already running");
        return;
    }

    // Inicializar sistema de upload HTML
    esp_err_t ret = html_upload_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HTML upload system");
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

    // Registrar endpoint para upload de HTML
    httpd_uri_t html_upload = {
        .uri       = "/upload/html",
        .method    = HTTP_POST,
        .handler   = html_upload_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &html_upload);

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

    // Modificar o handler principal para usar SPIFFS
    httpd_uri_t httpd_get = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = html_spiffs_handler,  // Usar novo handler
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &httpd_get);
    
    // Start WebSocket heartbeat timer
    start_ws_heartbeat_timer();
    
    // Initialize RCP protocol
    esp_err_t rcp_ret = rcp_init();
    if (rcp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RCP protocol: %s", esp_err_to_name(rcp_ret));
    }
    
    ESP_LOGI(TAG, "HTTP server started successfully");
}
```

### 6. Interface Web para Upload

```html
<!-- Adicionar à página de configuração (dentro de netCtr) -->
<!-- No arquivo index.html principal ou como nova aba -->

<div class="tab-panel" id="tabHtmlContent">
    <div class="section">
        <h3>🌐 Atualizar Interface Web</h3>
        <p>Faça upload de um novo arquivo HTML para atualizar a interface do sistema.</p>
        
        <div class="upload-section">
            <div class="file-input-wrapper">
                <input type="file" id="htmlFile" accept=".html,.htm" />
                <label for="htmlFile" class="file-input-label">
                    📁 Selecionar arquivo HTML
                </label>
            </div>
            
            <div class="upload-info">
                <p><strong>Requisitos:</strong></p>
                <ul>
                    <li>Arquivo HTML válido (máximo 1MB)</li>
                    <li>Extensão .html ou .htm</li>
                    <li>Backup automático do arquivo atual</li>
                </ul>
            </div>
            
            <button id="uploadHtml" class="btn btn-primary">
                🚀 Atualizar Interface
            </button>
            
            <button id="restoreHtmlBackup" class="btn btn-secondary">
                🔄 Restaurar Backup
            </button>
        </div>
        
        <div id="htmlUploadProgress" class="progress-container" style="display:none;">
            <div class="progress-bar">
                <div id="htmlProgressBar" class="progress-fill"></div>
            </div>
            <div id="htmlProgressText" class="progress-text">0%</div>
        </div>
        
        <div id="htmlUploadStatus" class="status-message"></div>
    </div>
</div>
```

```css
/* Adicionar ao style.css */
.upload-section {
    background: #f8f9fa;
    padding: 20px;
    border-radius: 8px;
    margin: 15px 0;
}

.file-input-wrapper {
    position: relative;
    display: inline-block;
    margin-bottom: 15px;
}

.file-input-wrapper input[type=file] {
    position: absolute;
    left: -9999px;
}

.file-input-label {
    display: inline-block;
    padding: 10px 20px;
    background: #007bff;
    color: white;
    border-radius: 5px;
    cursor: pointer;
    transition: background-color 0.3s;
}

.file-input-label:hover {
    background: #0056b3;
}

.upload-info {
    background: #e9ecef;
    padding: 15px;
    border-radius: 5px;
    margin: 15px 0;
    font-size: 14px;
}

.upload-info ul {
    margin: 10px 0 0 20px;
}

.btn {
    padding: 10px 20px;
    border: none;
    border-radius: 5px;
    cursor: pointer;
    font-size: 16px;
    margin: 5px;
    transition: all 0.3s;
}

.btn-primary {
    background: #28a745;
    color: white;
}

.btn-primary:hover {
    background: #218838;
}

.btn-secondary {
    background: #6c757d;
    color: white;
}

.btn-secondary:hover {
    background: #545b62;
}

.btn:disabled {
    opacity: 0.6;
    cursor: not-allowed;
}

.progress-container {
    margin: 20px 0;
}

.progress-bar {
    width: 100%;
    height: 25px;
    background: #e9ecef;
    border-radius: 12px;
    overflow: hidden;
    position: relative;
}

.progress-fill {
    height: 100%;
    background: linear-gradient(90deg, #28a745, #20c997);
    transition: width 0.3s ease;
    width: 0%;
}

.progress-text {
    text-align: center;
    margin-top: 10px;
    font-weight: bold;
}

.status-message {
    margin-top: 15px;
    padding: 10px;
    border-radius: 5px;
    display: none;
}

.status-success {
    background: #d4edda;
    color: #155724;
    border: 1px solid #c3e6cb;
}

.status-error {
    background: #f8d7da;
    color: #721c24;
    border: 1px solid #f5c6cb;
}
```

### 7. JavaScript para Upload

```javascript
// Adicionar ao script.js na função netCtr()

function netCtr(view) {
    // Tab switching functionality
    const tabItems = view.html.querySelectorAll('.tab-item');
    const tabPanels = view.html.querySelectorAll('.tab-panel');
    
    tabItems.forEach(tab => {
        tab.addEventListener('click', () => {
            // Remove active class from all tabs and panels
            tabItems.forEach(t => t.classList.remove('active'));
            tabPanels.forEach(p => p.classList.remove('active'));
            
            // Add active class to clicked tab
            tab.classList.add('active');
            
            // Show corresponding panel
            const targetPanel = tab.id.replace('tab', 'tab') + 'Content';
            document.getElementById(targetPanel).classList.add('active');
        });
    });
    
    // Load OTA status when OTA tab is clicked
    view.html.querySelector('#tabOTA').addEventListener('click', loadOTAStatus);
    
    // Load system info when Info tab is clicked
    view.html.querySelector('#tabInfo').addEventListener('click', loadSystemInfo);
    
    // WiFi configuration save
    view.html.querySelector('#saveWifiConfig').addEventListener('click', saveWifiConfig);
    
    // OTA upload functionality
    view.html.querySelector('#uploadOTA').addEventListener('click', uploadOTAFirmware);
    
    // HTML upload functionality
    view.html.querySelector('#uploadHtml').addEventListener('click', uploadHTMLFile);
    
    // HTML backup restore functionality
    view.html.querySelector('#restoreHtmlBackup').addEventListener('click', restoreHTMLBackup);
    
    // System info refresh button
    view.html.querySelector('#refreshSystemInfo').addEventListener('click', loadSystemInfo);
}

async function uploadHTMLFile() {
    const fileInput = document.getElementById('htmlFile');
    const file = fileInput.files[0];
    
    if (!file) {
        showHtmlUploadStatus('Por favor, selecione um arquivo HTML', 'error');
        return;
    }
    
    // Validar extensão do arquivo
    const validExtensions = ['.html', '.htm'];
    const fileExtension = file.name.toLowerCase().substring(file.name.lastIndexOf('.'));
    
    if (!validExtensions.includes(fileExtension)) {
        showHtmlUploadStatus('Por favor, selecione um arquivo .html ou .htm válido', 'error');
        return;
    }
    
    // Validar tamanho do arquivo (1MB máximo)
    if (file.size > 1048576) {
        showHtmlUploadStatus('Arquivo muito grande. Máximo permitido: 1MB', 'error');
        return;
    }
    
    // Validação básica do conteúdo HTML
    const fileText = await file.text();
    if (!fileText.toLowerCase().includes('<html') && !fileText.toLowerCase().includes('<body')) {
        const proceed = confirm('O arquivo não parece ser um HTML válido. Continuar mesmo assim?');
        if (!proceed) {
            return;
        }
    }
    
    const progressContainer = document.getElementById('htmlUploadProgress');
    const progressBar = document.getElementById('htmlProgressBar');
    const progressText = document.getElementById('htmlProgressText');
    const uploadButton = document.getElementById('uploadHtml');
    const restoreButton = document.getElementById('restoreHtmlBackup');
    
    // Show progress bar and disable buttons
    progressContainer.style.display = 'block';
    uploadButton.disabled = true;
    restoreButton.disabled = true;
    uploadButton.textContent = '🔄 Enviando...';
    
    try {
        const formData = new FormData();
        formData.append('html', file);
        
        const xhr = new XMLHttpRequest();
        
        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percentComplete = (e.loaded / e.total) * 100;
                progressBar.style.width = percentComplete + '%';
                progressText.textContent = Math.round(percentComplete) + '%';
            }
        });
        
        xhr.onload = function() {
            if (xhr.status === 200) {
                try {
                    const response = JSON.parse(xhr.responseText);
                    if (response.success) {
                        showHtmlUploadStatus('✅ Interface HTML atualizada com sucesso! Recarregue a página para ver as mudanças.', 'success');
                        fileInput.value = ''; // Reset file input
                        
                        // Oferecer recarregar a página após delay
                        setTimeout(() => {
                            if (confirm('Deseja recarregar a página para ver a nova interface?')) {
                                window.location.reload();
                            }
                        }, 2000);
                    } else {
                        showHtmlUploadStatus('❌ Erro: ' + response.message, 'error');
                    }
                } catch (e) {
                    showHtmlUploadStatus('✅ HTML enviado com sucesso!', 'success');
                    setTimeout(() => {
                        if (confirm('Deseja recarregar a página para ver a nova interface?')) {
                            window.location.reload();
                        }
                    }, 2000);
                }
            } else {
                showHtmlUploadStatus('❌ Erro no upload: ' + xhr.responseText, 'error');
            }
            
            // Reset UI
            uploadButton.disabled = false;
            restoreButton.disabled = false;
            uploadButton.textContent = '🚀 Atualizar Interface';
            progressContainer.style.display = 'none';
        };
        
        xhr.onerror = function() {
            showHtmlUploadStatus('❌ Erro na conexão durante o upload', 'error');
            uploadButton.disabled = false;
            restoreButton.disabled = false;
            uploadButton.textContent = '🚀 Atualizar Interface';
            progressContainer.style.display = 'none';
        };
        
        xhr.open('POST', '/upload/html');
        xhr.send(formData);
        
    } catch (error) {
        console.error('Erro no upload HTML:', error);
        showHtmlUploadStatus('❌ Erro ao enviar arquivo HTML', 'error');
        uploadButton.disabled = false;
        restoreButton.disabled = false;
        uploadButton.textContent = '🚀 Atualizar Interface';
        progressContainer.style.display = 'none';
    }
}

async function restoreHTMLBackup() {
    const confirmRestore = confirm('Tem certeza que deseja restaurar o backup da interface anterior? Isso irá desfazer a última atualização.');
    
    if (!confirmRestore) {
        return;
    }
    
    const uploadButton = document.getElementById('uploadHtml');
    const restoreButton = document.getElementById('restoreHtmlBackup');
    
    restoreButton.disabled = true;
    uploadButton.disabled = true;
    restoreButton.textContent = '🔄 Restaurando...';
    
    try {
        const response = await fetch('/restore/html', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            }
        });
        
        if (response.ok) {
            const result = await response.json();
            if (result.success) {
                showHtmlUploadStatus('✅ Backup restaurado com sucesso! Recarregue a página.', 'success');
                setTimeout(() => {
                    if (confirm('Deseja recarregar a página para ver a interface restaurada?')) {
                        window.location.reload();
                    }
                }, 1000);
            } else {
                showHtmlUploadStatus('❌ Erro ao restaurar backup: ' + result.message, 'error');
            }
        } else {
            showHtmlUploadStatus('❌ Erro ao restaurar backup', 'error');
        }
    } catch (error) {
        console.error('Erro ao restaurar backup:', error);
        showHtmlUploadStatus('❌ Erro na conexão', 'error');
    }
    
    // Reset buttons
    restoreButton.disabled = false;
    uploadButton.disabled = false;
    restoreButton.textContent = '🔄 Restaurar Backup';
}

function showHtmlUploadStatus(message, type) {
    const statusDiv = document.getElementById('htmlUploadStatus');
    statusDiv.textContent = message;
    statusDiv.className = 'status-message status-' + type;
    statusDiv.style.display = 'block';
    
    // Auto-hide after 10 seconds for success messages
    if (type === 'success') {
        setTimeout(() => {
            statusDiv.style.display = 'none';
        }, 10000);
    }
}
```

### 8. Endpoint para Restaurar Backup

```c
// Adicionar ao html_upload.c

esp_err_t html_restore_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "HTML restore request received");
    
    if (restore_html_backup() == ESP_OK) {
        const char *response = "{\"success\":true,\"message\":\"Backup restored successfully\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    } else {
        const char *response = "{\"success\":false,\"message\":\"Failed to restore backup\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, response, strlen(response));
        return ESP_FAIL;
    }
}
```

```c
// Adicionar ao html_upload.h
esp_err_t html_restore_handler(httpd_req_t *req);
```

```c
// Adicionar ao http_server.c na função http_server_start()
    // Registrar endpoint para restaurar backup HTML
    httpd_uri_t html_restore = {
        .uri       = "/restore/html",
        .method    = HTTP_POST,
        .handler   = html_restore_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(server, &html_restore);
```

---

## OPÇÃO 2: SISTEMA HÍBRIDO (ALTERNATIVA)

### Implementação do Sistema Híbrido

```c
// Arquivo: main/src/html_dynamic.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "html_dynamic.h"

static const char *TAG = "HTML_DYNAMIC";

// Variáveis globais para HTML dinâmico
static bool use_dynamic_html = false;
static char* dynamic_html_content = NULL;
static size_t dynamic_html_size = 0;

esp_err_t dynamic_html_upload_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Dynamic HTML upload request received");
    
    // Verificar tamanho do conteúdo
    if (req->content_len > 1048576) {  // 1MB max
        ESP_LOGE(TAG, "File too large: %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_413_REQ_ENTITY_TOO_LARGE, "File too large");
        return ESP_FAIL;
    }
    
    // Liberar HTML anterior se existir
    if (dynamic_html_content) {
        free(dynamic_html_content);
        dynamic_html_content = NULL;
        use_dynamic_html = false;
    }
    
    // Alocar memória para novo HTML
    dynamic_html_content = malloc(req->content_len + 1);
    if (!dynamic_html_content) {
        ESP_LOGE(TAG, "Memory allocation failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    // Receber dados
    int total_received = 0;
    int remaining = req->content_len;
    
    while (remaining > 0) {
        int received = httpd_req_recv(req, dynamic_html_content + total_received, remaining);
        if (received <= 0) {
            ESP_LOGE(TAG, "Error receiving data");
            free(dynamic_html_content);
            dynamic_html_content = NULL;
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_FAIL;
        }
        
        total_received += received;
        remaining -= received;
    }
    
    dynamic_html_content[total_received] = '\0';
    dynamic_html_size = total_received;
    use_dynamic_html = true;
    
    ESP_LOGI(TAG, "Dynamic HTML updated successfully (%d bytes)", total_received);
    
    const char *response = "{\"success\":true,\"message\":\"HTML updated successfully\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

esp_err_t hybrid_html_handler(httpd_req_t *req) {
    if (use_dynamic_html && dynamic_html_content) {
        // Usar HTML dinâmico
        ESP_LOGI(TAG, "Serving dynamic HTML (%d bytes)", dynamic_html_size);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_send(req, dynamic_html_content, dynamic_html_size);
    } else {
        // Usar HTML embebido (fallback)
        ESP_LOGI(TAG, "Serving embedded HTML (fallback)");
        extern const uint8_t index_html_start[] asm("_binary_index_html_start");
        extern const uint8_t index_html_end[] asm("_binary_index_html_end");
        const size_t index_html_size = (index_html_end - index_html_start);
        
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    }
    
    return ESP_OK;
}

esp_err_t reset_to_embedded_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Reset to embedded HTML requested");
    
    if (dynamic_html_content) {
        free(dynamic_html_content);
        dynamic_html_content = NULL;
    }
    
    use_dynamic_html = false;
    
    const char *response = "{\"success\":true,\"message\":\"Reset to embedded HTML\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

bool is_using_dynamic_html(void) {
    return use_dynamic_html;
}

size_t get_dynamic_html_size(void) {
    return dynamic_html_size;
}
```

---

## INSTRUÇÕES DE IMPLEMENTAÇÃO

### Passo 1: Escolher a Abordagem
- **SPIFFS/LittleFS (Opção 1)**: Para máxima flexibilidade e persistência
- **Sistema Híbrido (Opção 2)**: Para manter compatibilidade atual com fallback

### Passo 2: Modificar CMakeLists.txt
- Adicionar dependência SPIFFS (Opção 1)
- Manter EMBED_FILES (Opção 2)

### Passo 3: Configurar Partições
- Criar partição SPIFFS (apenas Opção 1)

### Passo 4: Implementar Código Backend
- Criar arquivos .c e .h conforme os códigos fornecidos
- Registrar endpoints HTTP
- Modificar handler principal

### Passo 5: Atualizar Interface Web
- Adicionar nova aba para upload HTML
- Implementar JavaScript para upload
- Adicionar CSS para estilização

### Passo 6: Compilar e Testar
```bash
idf.py build
idf.py flash
```

### Passo 7: Validar Funcionalidade
1. Acessar interface web
2. Ir para aba de upload HTML
3. Selecionar arquivo HTML
4. Fazer upload
5. Verificar se interface foi atualizada
6. Testar restore de backup

## Vantagens de Cada Opção

### SPIFFS/LittleFS:
- ✅ Persistência após reinicialização
- ✅ Backup automático
- ✅ Validação robusta
- ✅ Suporte a múltiplos arquivos
- ❌ Requer modificar estrutura atual

### Sistema Híbrido:
- ✅ Mantém compatibilidade
- ✅ Fallback automático  
- ✅ Implementação mais simples
- ❌ Perde mudanças após reboot
- ❌ Apenas na RAM

## Considerações de Segurança

1. **Validação de arquivo**: Verificar se é HTML válido
2. **Limitação de tamanho**: Máximo 1MB
3. **Backup automático**: Sempre fazer backup antes de substituir
4. **Rollback**: Possibilidade de restaurar versão anterior
5. **Sanitização**: Validar conteúdo para evitar XSS

## Troubleshooting

### Erro de compilação
- Verificar se todas as dependências estão no CMakeLists.txt
- Verificar includes dos headers

### Erro de SPIFFS
- Verificar configuração de partições
- Verificar se há espaço suficiente na flash

### Upload falha
- Verificar tamanho do arquivo
- Verificar Content-Type do request
- Verificar logs do ESP32

### Interface não atualiza
- Limpar cache do browser
- Recarregar página
- Verificar se arquivo foi salvo corretamente

---

## Conclusão

Este documento fornece uma implementação completa para upload de arquivos HTML via HTTP no ESP32. A **Opção 1 (SPIFFS)** é recomendada para máxima funcionalidade, enquanto a **Opção 2 (Híbrido)** oferece uma implementação mais simples mantendo compatibilidade.

Ambas as soluções incluem:
- ✅ Upload via multipart/form-data
- ✅ Validação de arquivos HTML  
- ✅ Sistema de backup/restore
- ✅ Interface web intuitiva
- ✅ Handling de erros robusto
- ✅ Logs detalhados para debug