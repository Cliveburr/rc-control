#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "config.h"

#define STORAGE_NAMESPACE "config"
#define MAIN_KEY "main_config"
#define VERSION 10
static const char *TAG = "config";

void config_set_default(config_data_t *config_data)
{
    ESP_LOGI(TAG, "config_set_default");

    config_data->net_mode = config_net_mode_station;

    strncpy(config_data->softap_ssid, "RC Control - 0\0", sizeof(config_data->softap_ssid) - 1);
    //config_data.softap_ssid[sizeof(config_data.softap_ssid) - 1] = '\0';

    strncpy(config_data->softap_password, "12345678\0", sizeof(config_data->softap_password) - 1);
    //config_data.softap_password[sizeof(config_data.softap_password) - 1] = '\0';

    config_data->softap_channel = 1;

    strncpy(config_data->station_ssid, "Matrix\0", sizeof(config_data->station_ssid) - 1);
    strncpy(config_data->station_password, "12346666\0", sizeof(config_data->station_password) - 1);

}

/******************************* PUBLIC METHODS *************************************/

void config_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

config_data_t config_load(void)
{
    config_data_t config_data;
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
        return config_data;
    }

    size_t required_size = 0;
    err = nvs_get_blob(my_handle, MAIN_KEY, NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
    }
    
    if (required_size == 0)
    {
        config_set_default(&config_data);
    }
    else
    {
        //uint32_t* run_time = malloc(required_size);
        err = nvs_get_blob(my_handle, MAIN_KEY, &config_data, &required_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
            //free(run_time);
            return config_data;
        }

        if (config_data.version != VERSION)
        {
            config_set_default(&config_data);
        }
        //free(run_time);
    }

    // config_data_t config_data; // = malloc(sizeof(config_data_t));
    // config_data.test_str = "um teste qualquer";
    nvs_close(my_handle);

    return config_data;
}

void config_save(config_data_t config_data)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
        return;
    }

    err = nvs_set_blob(my_handle, MAIN_KEY, &config_data, sizeof(config_data_t));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) reading data from NVS!\n", esp_err_to_name(err));
    }

    nvs_close(my_handle);
}