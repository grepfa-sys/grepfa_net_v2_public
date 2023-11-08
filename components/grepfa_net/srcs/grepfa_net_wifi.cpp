#include "grepfa_net.h"
#include <esp_wifi.h>
#include <cstring>

static const char *TAG = "WIFI";

const int WIFI_CONNECTED_EVENT = BIT0;

EventGroupHandle_t  GrepfaNet::sta_event_group = nullptr;
char GrepfaNet::dev_service_name[64] = {};

void GrepfaNet::prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    static int retries;

    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                auto *wifi_sta_cfg = (wifi_sta_config_t *) event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials"
                              "\n\tSSID     : %s\n\tPassword : %s",
                         (const char *) wifi_sta_cfg->ssid,
                         (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                auto *reason = (wifi_prov_sta_fail_reason_t *) event_data;
                ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s"
                              "\n\tPlease reset to factory and retry provisioning",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                         "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
                retries++;
                if (retries >= PROV_MGR_MAX_RETRY_CNT) {
                    ESP_LOGI(TAG, "Failed to connect with provisioned AP, reseting provisioned credentials");
                    wifi_prov_mgr_reset_sm_state_on_failure();
                    retries = 0;
                }
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                retries = 0;
                break;
            case WIFI_PROV_END:
                /* De-initialize manager once provisioning is finished */
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
                esp_wifi_connect();
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        /* Signal main application to continue execution */
        xEventGroupSetBits(sta_event_group, WIFI_CONNECTED_EVENT);

    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
            case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
                ESP_LOGI(TAG, "BLE transport: Connected!");
                break;
            case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
                ESP_LOGI(TAG, "BLE transport: Disconnected!");
                break;
            default:
                break;
        }
    }
}


void GrepfaNet::prov_init() {
    sta_event_group = xEventGroupCreate();
    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, nullptr);
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, nullptr));

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
    bool provisioned = false;

    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        ESP_LOGI(TAG, "Starting provisioning");
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = "grepfa1234!";
        wifi_prov_security1_params_t *sec_params = pop;
        const char *service_key = nullptr;
        uint8_t custom_service_uuid[] = {
                /* LSB <---------------------------------------
                 * ---------------------------------------> MSB */
                0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
                0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };

        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
        ESP_ERROR_CHECK(
                wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, dev_service_name, service_key));
    }else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        /* We don't need the manager as device is already provisioned,
         * so let's release it's resources */
        wifi_prov_mgr_deinit();

        /* Start Wi-Fi station */
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}


void GrepfaNet::SetDeviceServiceName(const char *name) {
    strncpy(dev_service_name, name, 64);
}

void GrepfaNet::ResetProvisioning() {
    wifi_prov_mgr_reset_provisioning();
}
