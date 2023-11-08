#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <esp_netif.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_eth_spec.h>
#include <esp_mac.h>
#include <esp_eth_driver.h>
#include <esp_wifi_types.h>

#include <wifi_provisioning/scheme_ble.h>

#include <functional>
#include "ip_type.h"

#define PROV_MGR_MAX_RETRY_CNT 5

typedef struct {
    std::string ssid;
    std::string pw;
    uint8_t bssid[6];
} grepfa_wifi_ap_info_t;

class GrepfaNet {
private:

    // netif
    static bool is_nif_init;

    static esp_netif_t *eth_nif;
//    static esp_netif_t *sta_nif;

    static esp_event_handler_instance_t instance_nif;

    static EventGroupHandle_t eth_nif_event_group;
    static EventGroupHandle_t sta_nif_event_group;

    static void netif_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);


    static esp_err_t netif_init();

    // eth
    static esp_eth_handle_t eth_handler;
    static esp_event_handler_instance_t instance_eth;
    static uint8_t eth_mac_addr[ETH_ADDR_LEN];
    static int pin_mosi;
    static int pin_miso;
    static int pin_ret;
    static int pin_sck;
    static int pin_cs;
    static int pin_interrupt;
    static int host_id;
    static int eth_spi_clock_mhz; // 8

    static esp_err_t eth_init(int id, int mosi, int miso, int sck, int cs, int ret, int interrupt);

    static esp_err_t spi_init();

    static esp_err_t h_eth_init();

    static esp_err_t eth_netif_init();

    static esp_err_t eth_start();

    static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);


    // wifi prov
    static char dev_service_name[64];

    static EventGroupHandle_t sta_event_group;

    static void prov_init();

    static void prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

public:
    // init
    static esp_err_t Init(int id, int mosi, int miso, int sck, int cs, int ret, int interrupt);

    static esp_err_t Start();

    // eth
    static esp_netif_t *EthGetNetif();

    static esp_err_t EthSetDynamic(bool wait);

    static esp_err_t EthSetIP(IpAddr ip, IpAddr gateway, IpAddr subnet);

    static esp_err_t EthGetIP(IpAddr &ip, IpAddr &gateway, IpAddr &subnet);

    static esp_err_t EthSetDNS(IpAddr dns1, IpAddr dns2);

    static esp_err_t EthGetDNS(IpAddr &dns1, IpAddr &dns2);

    static esp_err_t EthGetIpWait(TickType_t time);

    static bool EthIsNifGotIP();

    static esp_err_t EthGetMac(char *mac);

    static esp_err_t EthSetHostname(const char *hostname);

    static esp_err_t EthGetHostname(const char **hostname);


    // wifi
    static void SetDeviceServiceName(const char *name);
    static void ResetProvisioning();

//    static esp_netif_t *StaGetNetif();
//    static esp_err_t StaSetDynamic(bool wait);
//    static esp_err_t StaSetIP(IpAddr ip, IpAddr gateway, IpAddr subnet);
//    static esp_err_t StaGetIP(IpAddr &ip, IpAddr &gateway, IpAddr &subnet);
//    static esp_err_t StaSetDNS(IpAddr dns1, IpAddr dns2);
//    static esp_err_t StaGetDNS(IpAddr &dns1, IpAddr &dns2);
//    static esp_err_t StaGetIpWait(TickType_t time);
//    static bool StaIsNifGotIP();
//    static esp_err_t StaGetMac(char *mac);
//    static esp_err_t StaSetHostname(const char *hostname);
//    static esp_err_t StaGetHostname(const char **hostname);

};