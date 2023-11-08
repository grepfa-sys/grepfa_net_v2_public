#include <grepfa_net.h>

static const char* TAG = "NET";

esp_err_t GrepfaNet::Init(int id, int mosi, int miso, int sck, int cs, int ret, int interrupt) {

    ESP_LOGI(TAG, "nif initialization start");
    esp_err_t err = netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nif initialization fail - %04x", err);
        return err;
    }

    ESP_LOGI(TAG, "eth initialization start");
    err = eth_init(id, mosi, miso, sck, cs, ret, interrupt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth initialization fail - %04x", err);
        return err;
    }

    ESP_LOGI(TAG, "wifi initialization start");
    prov_init();

    return ESP_OK;
}

esp_err_t GrepfaNet::Start() {
    ESP_LOGI(TAG, "eth service start");

    esp_err_t err = eth_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth start fail - %04x", err);
        return err;
    }


    return ESP_OK;
}