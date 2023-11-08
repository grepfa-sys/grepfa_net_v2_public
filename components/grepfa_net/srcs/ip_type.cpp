//
// Created by vl0011 on 23. 8. 27.
//

#include <lwip/ip4_addr.h>
#include "ip_type.h"

IpAddr::IpAddr(const char *str) {
    ip = ipaddr_addr(str);
}

IpAddr::IpAddr(const std::string &str) {
    ip = ipaddr_addr(str.c_str());
}

void IpAddr::toCStr(char *buf, int buf_len) {
    ip4_addr_t addr;
    addr.addr = ip;

    ip4addr_ntoa_r(&addr, buf, buf_len);
}

std::string IpAddr::toStr() {
    ip4_addr_t addr;
    addr.addr = ip;
    return ip4addr_ntoa(&addr);
}

uint32_t IpAddr::getUint32() {
    return ip;
}

esp_ip4_addr IpAddr::getIp4Addr() {
    return esp_ip4_addr{
            .addr = ip
    };
}

IpAddr::IpAddr(uint32_t ip) {
    this->ip = ip;
}

IpAddr &IpAddr::operator=(esp_ip4_addr_t &val) {
    this->ip = val.addr;
    return *this;
}

IpAddr &IpAddr::operator=(uint32_t val) {
    this->ip = val;
    return *this;
}

IpAddr &IpAddr::operator=(const char *ipStr) {
    this->ip = ipaddr_addr(ipStr);
    return *this;
}

IpAddr &IpAddr::operator=(const std::string &ipStr) {
    this->ip = ipaddr_addr(ipStr.c_str());
    return *this;
}


