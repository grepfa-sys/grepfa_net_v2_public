#include <esp_log.h>
#include <esp_event.h>
#include <lwip/ip4_addr.h>
#include <lwip/ip_addr.h>

#include "grepfa_net.h"

static const char* TAG = "NIF";

enum {
    NIF_FAIL = BIT0,
    NIF_GOT_IP = BIT1,
    NIF_LOST_IP = BIT2,
};

bool GrepfaNet::is_nif_init = false;
esp_netif_t* GrepfaNet::eth_nif = nullptr;
//esp_netif_t* GrepfaNet::sta_nif = nullptr;
esp_event_handler_instance_t GrepfaNet::instance_nif = nullptr;
EventGroupHandle_t GrepfaNet::eth_nif_event_group = nullptr;
EventGroupHandle_t GrepfaNet::sta_nif_event_group = nullptr;

void GrepfaNet::netif_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

}

esp_netif_t *GrepfaNet::EthGetNetif() {
    return eth_nif;
}

esp_err_t GrepfaNet::EthSetDynamic(bool wait) {
    if (eth_nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }

    esp_err_t err;
//    esp_netif_ip_info_t ip_cfg;
//    ip_cfg.ip.addr = 0;
//    ip_cfg.gw.addr = 0;
//    ip_cfg.netmask.addr = 0;
//    err = esp_netif_set_ip_info(nif, &ip_cfg);
//    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "delete ip fail - 0x04%x", err);
//        return err;
//    }

    err = esp_netif_dhcpc_start(eth_nif);
    if (!(err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)) {
        ESP_LOGE(TAG, "dhcp client start fail - 0x04%x", err);
    }

    if (wait) {
        GrepfaNet::EthGetIpWait(portMAX_DELAY);
    }

    return ESP_OK;
}

esp_err_t GrepfaNet::EthSetIP(IpAddr ip, IpAddr gateway, IpAddr subnet) {
    if (eth_nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }
    esp_err_t err = esp_netif_dhcpc_stop(eth_nif);
    if (!(err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)) {
        ESP_LOGE(TAG, "dhcp client stop fail - %04x", err);
        return err;
    }

    esp_netif_ip_info_t ip_cfg;
    ip_cfg.ip = ip.getIp4Addr();
    ip_cfg.gw = gateway.getIp4Addr();
    ip_cfg.netmask = subnet.getIp4Addr();

    err = esp_netif_set_ip_info(eth_nif, &ip_cfg);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set ip fail - %04x", err);
        return err;
    }
    return ESP_OK;
}

esp_err_t GrepfaNet::EthGetIP(IpAddr &ip, IpAddr &gateway, IpAddr &subnet) {
    if (eth_nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }

    esp_netif_ip_info_t nip;
    esp_err_t err = esp_netif_get_ip_info(eth_nif, &nip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get ip fail - %04x", err);
        return err;
    }

    ip = nip.ip;
    gateway = nip.gw;
    subnet = nip.netmask;

    return ESP_OK;
}

esp_err_t GrepfaNet::EthSetDNS(IpAddr dns1, IpAddr dns2) {
    if (eth_nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }

    esp_err_t err;
    if (dns1.getUint32() && (dns1.getUint32() != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = dns1.getUint32();
        dns.ip.type = IPADDR_TYPE_V4;
        err = esp_netif_set_dns_info(eth_nif, ESP_NETIF_DNS_MAIN, &dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "main dns set fail - %04x", err);
            return err;
        }
    }

    if (dns2.getUint32() && (dns2.getUint32() != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = dns2.getUint32();
        dns.ip.type = IPADDR_TYPE_V4;
        err = esp_netif_set_dns_info(eth_nif, ESP_NETIF_DNS_BACKUP, &dns);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "backup dns set fail - %04x", err);
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t GrepfaNet::EthGetDNS(IpAddr &dns1, IpAddr &dns2) {
    if (eth_nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }

    esp_netif_dns_info_t dns_info;
    esp_err_t err = esp_netif_get_dns_info(eth_nif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get main dns fail - %04x", err);
        return err;
    }

    dns1 = dns_info.ip.u_addr.ip4;

    err = esp_netif_get_dns_info(eth_nif, ESP_NETIF_DNS_BACKUP, &dns_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get backup dns fail - %04x", err);
        return err;
    }

    dns2 = dns_info.ip.u_addr.ip4;
    return ESP_OK;
}

esp_err_t GrepfaNet::EthGetIpWait(TickType_t time) {
    xEventGroupClearBits(eth_nif_event_group, NIF_GOT_IP | NIF_FAIL);
    EventBits_t bits = xEventGroupWaitBits(eth_nif_event_group,
                                           NIF_GOT_IP | NIF_FAIL,
                                           pdFALSE,
                                           pdFALSE,
                                           time);
    if (bits & NIF_GOT_IP) {
        ESP_LOGI(TAG, "ip set");
        return ESP_OK;
    }
    ESP_LOGW(TAG, "ip set fail");
    return ESP_FAIL;
}

bool GrepfaNet::EthIsNifGotIP() {
    EventBits_t uxBits = xEventGroupGetBits(eth_nif_event_group);
    return uxBits & NIF_GOT_IP;
}

esp_err_t GrepfaNet::EthGetMac(char *mac) {
    esp_netif_t *nif = eth_nif;
    if (nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }
    return esp_netif_get_mac(nif, reinterpret_cast<uint8_t *>(mac));
}

esp_err_t GrepfaNet::EthSetHostname(const char *hostname) {
    esp_netif_t *nif = eth_nif;
    if (nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }

    return esp_netif_set_hostname(nif, hostname);
}

esp_err_t GrepfaNet::EthGetHostname(const char **hostname) {
    esp_netif_t *nif = eth_nif;
    if (nif == nullptr) {
        ESP_LOGE(TAG, "netif not set");
        return ESP_FAIL;
    }

    return esp_netif_get_hostname(nif, hostname);
}


//esp_netif_t *GrepfaNet::StaGetNetif() {
//    return sta_nif;
//}
//
//esp_err_t GrepfaNet::StaSetDynamic(bool wait) {
//    if (sta_nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//
//    esp_err_t err;
////    esp_netif_ip_info_t ip_cfg;
////    ip_cfg.ip.addr = 0;
////    ip_cfg.gw.addr = 0;
////    ip_cfg.netmask.addr = 0;
////    err = esp_netif_set_ip_info(nif, &ip_cfg);
////    if (err != ESP_OK) {
////        ESP_LOGE(TAG, "delete ip fail - 0x04%x", err);
////        return err;
////    }
//
//    err = esp_netif_dhcpc_start(sta_nif);
//    if (!(err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED)) {
//        ESP_LOGE(TAG, "dhcp client start fail - 0x04%x", err);
//    }
//
//    if (wait) {
//        GrepfaNet::EthGetIpWait(portMAX_DELAY);
//    }
//
//    return ESP_OK;
//}
//
//esp_err_t GrepfaNet::StaSetIP(IpAddr ip, IpAddr gateway, IpAddr subnet) {
//    if (sta_nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//    esp_err_t err = esp_netif_dhcpc_stop(sta_nif);
//    if (!(err == ESP_OK || err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)) {
//        ESP_LOGE(TAG, "dhcp client stop fail - %04x", err);
//        return err;
//    }
//
//    esp_netif_ip_info_t ip_cfg;
//    ip_cfg.ip = ip.getIp4Addr();
//    ip_cfg.gw = gateway.getIp4Addr();
//    ip_cfg.netmask = subnet.getIp4Addr();
//
//    err = esp_netif_set_ip_info(sta_nif, &ip_cfg);
//
//    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "set ip fail - %04x", err);
//        return err;
//    }
//    return ESP_OK;
//}
//
//esp_err_t GrepfaNet::StaGetIP(IpAddr &ip, IpAddr &gateway, IpAddr &subnet) {
//    if (sta_nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//
//    esp_netif_ip_info_t nip;
//    esp_err_t err = esp_netif_get_ip_info(sta_nif, &nip);
//    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "get ip fail - %04x", err);
//        return err;
//    }
//
//    ip = nip.ip;
//    gateway = nip.gw;
//    subnet = nip.netmask;
//
//    return ESP_OK;
//}
//
//esp_err_t GrepfaNet::StaSetDNS(IpAddr dns1, IpAddr dns2) {
//    if (sta_nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//
//    esp_err_t err;
//    if (dns1.getUint32() && (dns1.getUint32() != IPADDR_NONE)) {
//        esp_netif_dns_info_t dns;
//        dns.ip.u_addr.ip4.addr = dns1.getUint32();
//        dns.ip.type = IPADDR_TYPE_V4;
//        err = esp_netif_set_dns_info(sta_nif, ESP_NETIF_DNS_MAIN, &dns);
//        if (err != ESP_OK) {
//            ESP_LOGE(TAG, "main dns set fail - %04x", err);
//            return err;
//        }
//    }
//
//    if (dns2.getUint32() && (dns2.getUint32() != IPADDR_NONE)) {
//        esp_netif_dns_info_t dns;
//        dns.ip.u_addr.ip4.addr = dns2.getUint32();
//        dns.ip.type = IPADDR_TYPE_V4;
//        err = esp_netif_set_dns_info(sta_nif, ESP_NETIF_DNS_BACKUP, &dns);
//        if (err != ESP_OK) {
//            ESP_LOGE(TAG, "backup dns set fail - %04x", err);
//            return err;
//        }
//    }
//    return ESP_OK;
//}
//
//esp_err_t GrepfaNet::StaGetDNS(IpAddr &dns1, IpAddr &dns2) {
//    if (sta_nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//
//    esp_netif_dns_info_t dns_info;
//    esp_err_t err = esp_netif_get_dns_info(sta_nif, ESP_NETIF_DNS_MAIN, &dns_info);
//    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "get main dns fail - %04x", err);
//        return err;
//    }
//
//    dns1 = dns_info.ip.u_addr.ip4;
//
//    err = esp_netif_get_dns_info(sta_nif, ESP_NETIF_DNS_BACKUP, &dns_info);
//    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "get backup dns fail - %04x", err);
//        return err;
//    }
//
//    dns2 = dns_info.ip.u_addr.ip4;
//    return ESP_OK;
//}
//
//esp_err_t GrepfaNet::StaGetIpWait(TickType_t time) {
//    xEventGroupClearBits(sta_nif_event_group, NIF_GOT_IP | NIF_FAIL);
//    EventBits_t bits = xEventGroupWaitBits(sta_nif_event_group,
//                                           NIF_GOT_IP | NIF_FAIL,
//                                           pdFALSE,
//                                           pdFALSE,
//                                           time);
//    if (bits & NIF_GOT_IP) {
//        ESP_LOGI(TAG, "ip set");
//        return ESP_OK;
//    }
//    ESP_LOGW(TAG, "ip set fail");
//    return ESP_FAIL;
//}
//
//bool GrepfaNet::StaIsNifGotIP() {
//    EventBits_t uxBits = xEventGroupGetBits(sta_nif_event_group);
//    return uxBits & NIF_GOT_IP;
//}
//
//esp_err_t GrepfaNet::StaGetMac(char *mac) {
//    esp_netif_t *nif = sta_nif;
//    if (nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//    return esp_netif_get_mac(nif, reinterpret_cast<uint8_t *>(mac));
//}
//
//esp_err_t GrepfaNet::StaSetHostname(const char *hostname) {
//    esp_netif_t *nif = sta_nif;
//    if (nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//
//    return esp_netif_set_hostname(nif, hostname);
//}
//
//esp_err_t GrepfaNet::StaGetHostname(const char **hostname) {
//    esp_netif_t *nif = sta_nif;
//    if (nif == nullptr) {
//        ESP_LOGE(TAG, "netif not set");
//        return ESP_FAIL;
//    }
//
//    return esp_netif_get_hostname(nif, hostname);
//}

esp_err_t GrepfaNet::netif_init() {
    esp_err_t err;

    // nif
    if (!is_nif_init) {
        ESP_LOGI(TAG, "netif init start");
        err = esp_netif_init();

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "netif initialization fail - %04x", err);
            return err;
        }


        esp_event_handler_instance_register(
                IP_EVENT,
                ESP_EVENT_ANY_ID,
                &netif_event_handler,
                nullptr,
                &instance_nif
        );

        eth_nif_event_group = xEventGroupCreate();
        sta_nif_event_group = xEventGroupCreate();

        is_nif_init = true;
    }


    return ESP_OK;
}