#include "wifi_ap.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_hosted.h"
#include <string.h>

static const char *TAG = "wifi_ap";
_Static_assert(sizeof(CONFIG_GRID_BOARD_AP_SSID) > 1 && sizeof(CONFIG_GRID_BOARD_AP_SSID) <= 33,
               "AP name must contain 1-32 bytes");
_Static_assert(sizeof(CONFIG_GRID_BOARD_AP_PASSWORD) >= 9 && sizeof(CONFIG_GRID_BOARD_AP_PASSWORD) <= 64,
               "WPA2 password must contain 8-63 characters");

esp_err_t wifi_ap_start(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "network init");
    // ESP-Hosted may already have created the default loop in its constructor.
    esp_err_t loop_result = esp_event_loop_create_default();
    if (loop_result != ESP_OK && loop_result != ESP_ERR_INVALID_STATE) return loop_result;
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(netif, ESP_ERR_NO_MEM, TAG, "AP netif");
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_stop(netif), TAG, "stop DHCP");
    esp_netif_ip_info_t ip = {0};
    ip.ip.addr = ESP_IP4TOADDR(192, 168, 4, 1);
    ip.gw.addr = ip.ip.addr;
    ip.netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0);
    ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(netif, &ip), TAG, "fixed AP IP");
    ESP_RETURN_ON_ERROR(esp_netif_dhcps_start(netif), TAG, "start DHCP");

    // P4 has no radio: esp_wifi_* is routed through esp_wifi_remote to C61.
    ESP_RETURN_ON_ERROR(esp_hosted_init(), TAG, "hosted SPI transport");
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "remote Wi-Fi init");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi RAM settings");
    wifi_config_t config = {0};
    memcpy(config.ap.ssid, CONFIG_GRID_BOARD_AP_SSID, sizeof(CONFIG_GRID_BOARD_AP_SSID) - 1);
    memcpy(config.ap.password, CONFIG_GRID_BOARD_AP_PASSWORD, sizeof(CONFIG_GRID_BOARD_AP_PASSWORD) - 1);
    config.ap.ssid_len = sizeof(CONFIG_GRID_BOARD_AP_SSID) - 1;
    config.ap.channel = CONFIG_GRID_BOARD_AP_CHANNEL;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    config.ap.pmf_cfg.capable = true;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "AP mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &config), TAG, "AP config");
    // The radio is remote: verify the applied settings instead of treating a
    // successful RPC acknowledgement as proof that the requested AP is active.
    wifi_config_t applied = {0};
    ESP_RETURN_ON_ERROR(esp_wifi_get_config(WIFI_IF_AP, &applied), TAG, "AP config readback");
    size_t ssid_length = applied.ap.ssid_len ? applied.ap.ssid_len :
                         strnlen((const char *)applied.ap.ssid, sizeof(applied.ap.ssid));
    ESP_RETURN_ON_FALSE(ssid_length == sizeof(CONFIG_GRID_BOARD_AP_SSID) - 1 &&
                        memcmp(applied.ap.ssid, CONFIG_GRID_BOARD_AP_SSID, ssid_length) == 0 &&
                        applied.ap.ssid_hidden == 0, ESP_ERR_INVALID_RESPONSE, TAG,
                        "C61 AP configuration does not match requested settings");
    ESP_LOGI(TAG, "AP config confirmed: SSID=%.*s, channel=%u, hidden=%u, beacon=%u ms",
             (int)ssid_length, applied.ap.ssid, applied.ap.channel,
             applied.ap.ssid_hidden, applied.ap.beacon_interval);
    // Explicitly initialize the hosted radio's 2.4 GHz protocol configuration.
    // Some C61 firmware also reports the 11ax bit after accepting b/g/n here.
    ESP_RETURN_ON_ERROR(esp_wifi_set_protocol(WIFI_IF_AP,
                        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N), TAG,
                        "AP protocol");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "AP start");
    wifi_mode_t mode = WIFI_MODE_NULL;
    uint8_t protocol = 0;
    uint8_t mac[6] = {0};
    int8_t power = 0;
    ESP_RETURN_ON_ERROR(esp_wifi_get_mode(&mode), TAG, "AP mode readback");
    ESP_RETURN_ON_ERROR(esp_wifi_get_protocol(WIFI_IF_AP, &protocol), TAG, "AP protocol readback");
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_AP, mac), TAG, "AP MAC readback");
    ESP_RETURN_ON_ERROR(esp_wifi_get_max_tx_power(&power), TAG, "AP TX power readback");
    ESP_LOGI(TAG, "Radio confirmed: mode=%u, protocol=0x%02x, TX power=%d quarter-dBm",
             (unsigned)mode, protocol, power);
    ESP_LOGI(TAG, "AP MAC=%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "AP ready; configuration URL: http://" GRID_BOARD_IP);
    return ESP_OK;
}
