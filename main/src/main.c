#include <esp_log.h>
//#include "../../../../v5.2/esp-idf/components/soc/esp32c2/include/soc/rtc_cntl_reg.h"

#include "config.h"
#include "net.h"
#include "ota.h"
#include "led_control.h"

// #include <sys/unistd.h>
// #include "esp_log.h"
// #include "esp_system.h"

// extern const uint8_t index_html_start[] asm("_binary_wwwroot_index_html_start");
// extern const uint8_t index_html_end[]   asm("_binary_wwwroot_index_html_end");
// extern const uint8_t index_html_start[] asm("_binary_index_html_start");
// extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

void app_main(void)
{
    //TODO: remove this line and fix error: Brownout detector was triggered
    //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // const size_t index_html_size = (index_html_end - index_html_start);
    // ESP_LOGI("test", "file size: %d", index_html_size);

    config_init();

    led_control_init();

    ota_init();

    net_init();

    // config_data_t config_data = config_load();

    // ESP_LOGI(TAG, "Count: %d!\n", config_data.count++);

    // config_save(config_data);
}