#pragma once

#include <esp_netif_types.h>
#include <string>

class IpAddr {
private:
    uint32_t ip = 0;

public:
    IpAddr() = default;

    IpAddr(uint32_t ip);

    IpAddr(const char *str);

    IpAddr(const std::string &str);

    void toCStr(char *buf, int buf_len);

    std::string toStr();

    uint32_t getUint32();

    esp_ip4_addr getIp4Addr();

    IpAddr &operator=(esp_ip4_addr_t &val);

    IpAddr &operator=(uint32_t val);

    IpAddr &operator=(const char *ipStr);

    IpAddr &operator=(const std::string &ipStr);
};