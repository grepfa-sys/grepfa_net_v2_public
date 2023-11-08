
#include <nvs_flash.h>
#include <esp_event.h>
#include "grepfa_net.h"

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    GrepfaNet::SetDeviceServiceName("hell_o");

    GrepfaNet::Init(SPI2_HOST, 11, 13, 12, 10, 7, 9);
    GrepfaNet::Start();

    while (true) {
        vTaskDelay(1000/portTICK_PERIOD_MS);
        ESP_LOGI("1", "hello");
    }
}
