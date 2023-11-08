#include <grepfa_net.h>
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <esp_eth_mac.h>
#include <esp_eth_phy.h>
#include <esp_eth_driver.h>
#include <esp_eth.h>
#include <esp_event.h>

static const char* TAG = "ETH";

esp_eth_handle_t GrepfaNet::eth_handler = nullptr;
esp_event_handler_instance_t GrepfaNet::instance_eth = nullptr;
uint8_t GrepfaNet::eth_mac_addr[ETH_ADDR_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
int GrepfaNet::pin_mosi = 0;
int GrepfaNet::pin_miso = 0;
int GrepfaNet::pin_ret = 0;
int GrepfaNet::pin_sck = 0;
int GrepfaNet::pin_cs = 0;
int GrepfaNet::pin_interrupt = 0;
int GrepfaNet::host_id = 0;
int GrepfaNet::eth_spi_clock_mhz = 8; // 8

esp_err_t GrepfaNet::eth_init(int id, int mosi, int miso, int sck, int cs, int ret, int interrupt) {
    uint8_t base_mac_addr[ETH_ADDR_LEN];
    esp_err_t err = esp_efuse_mac_get_default(base_mac_addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get EFUSE MAC fail - %04x", err);
        return err;
    }

    esp_derive_local_mac(eth_mac_addr, base_mac_addr);

    pin_mosi = mosi;
    pin_miso = miso;
    pin_cs = cs;
    pin_interrupt = interrupt;
    pin_ret = ret;
    pin_sck = sck;
    host_id = id;


    err = spi_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi init fail - %04x", err);
        return err;
    }

    err = h_eth_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth init fail - %04x", err);
        return err;
    }

    err = eth_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "netif init fail - %04x", err);
        return err;
    }
    return ESP_OK;
}

esp_err_t GrepfaNet::spi_init() {
    esp_err_t err = ESP_OK;
    err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "GPIO ISR handler has been already installed");
            err = ESP_OK; // ISR handler has been already installed so no issues
        } else {
            ESP_LOGE(TAG, "GPIO ISR handler install failed");
            return err;
        }
    }


    spi_bus_config_t buscfg = {
            .mosi_io_num = pin_mosi,
            .miso_io_num = pin_miso,
            .sclk_io_num = pin_sck,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
    };

    err = spi_bus_initialize(static_cast<spi_host_device_t>(host_id), &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI init fail");
        return err;
    }
    return ESP_OK;
}

esp_err_t GrepfaNet::h_eth_init() {
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = -1;
    phy_config.reset_gpio_num = pin_ret;

    spi_device_interface_config_t spi_devcfg = {
            .mode = 0,
            .clock_speed_hz = eth_spi_clock_mhz * 1000 * 1000,
            .spics_io_num = pin_cs,
            .queue_size = 20,
    };

    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(static_cast<spi_host_device_t>(host_id),
                                                               &spi_devcfg);
    w5500_config.int_gpio_num = pin_interrupt;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);


    esp_err_t err = esp_eth_driver_install(&eth_config_spi, &eth_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth driver install fail");
        if (eth_handler != nullptr) {
            esp_eth_driver_uninstall(eth_handler);
        }
        if (mac != nullptr) {
            mac->del(mac);
        }
        if (phy != nullptr) {
            phy->del(phy);
        }
        return err;
    }
    err = esp_eth_ioctl(eth_handler, ETH_CMD_S_MAC_ADDR, eth_mac_addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "config mac fail");
        if (eth_handler != nullptr) {
            esp_eth_driver_uninstall(eth_handler);
        }
        if (mac != nullptr) {
            mac->del(mac);
        }
        if (phy != nullptr) {
            phy->del(phy);
        }
        return err;
    }

    return ESP_OK;
}

esp_err_t GrepfaNet::eth_netif_init() {
    esp_netif_config_t ecfg = ESP_NETIF_DEFAULT_ETH();
    eth_nif = esp_netif_new(&ecfg);

    esp_err_t err = esp_netif_attach(eth_nif, esp_eth_new_netif_glue(eth_handler));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth netif attach fail");
        return err;
    }

    esp_event_handler_instance_register(
            ETH_EVENT,
            ESP_EVENT_ANY_ID,
            &eth_event_handler,
            nullptr,
            &instance_eth
    );

    return ESP_OK;
}

esp_err_t GrepfaNet::eth_start() {
    esp_err_t err;
    err = esp_eth_start(eth_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth start fail");
        return err;
    }
    return ESP_OK;
}

void GrepfaNet::eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base != ETH_EVENT) {
        return;
    }

    auto id = static_cast<eth_event_t>(event_id);
    switch (id) {
        case ETHERNET_EVENT_START:
            ESP_LOGI(TAG, "ethernet start");
            break;
        case ETHERNET_EVENT_STOP:
            ESP_LOGI(TAG, "ethernet stop");
            break;
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ethernet connected");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "ethernet disconnected");
            break;
    }


    // PASS
}



