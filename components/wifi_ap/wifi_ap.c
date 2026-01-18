#include <string.h>
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "wifi_ap.h"
#include "wifi.h"

static const char *TAG = "WIFI_AP";

#define NVS_NAMESPACE "storage"     // keys for nvs
#define KEY_SSID      "wifi_ssid"
#define KEY_PASS      "wifi_pass"

// website
const char* html_page = "<html><body><h1>Konfiguracja ESP32</h1><form action='/save' method='POST'>"
                        "SSID: <input name='ssid'><br>PASS: <input name='pass' type='password'><br>"
                        "<input type='submit' value='Zapisz'></form></body></html>";

static EventGroupHandle_t wifi_ap_event_group; 
#define WIFI_AP_ACTIVE_BIT (1UL << 0)

bool wifi_is_ap_active(void) {
    if (wifi_ap_event_group == NULL) return false;
    EventBits_t uxBits = xEventGroupGetBits(wifi_ap_event_group);
    return (uxBits & WIFI_AP_ACTIVE_BIT) != 0;
}

void wifi_set_ap_active_bit(bool active){
    if (wifi_ap_event_group == NULL) wifi_ap_event_group = xEventGroupCreate();
    
    if (active) xEventGroupSetBits(wifi_ap_event_group, WIFI_AP_ACTIVE_BIT); 
    else xEventGroupClearBits(wifi_ap_event_group, WIFI_AP_ACTIVE_BIT);
}   

esp_err_t save_wifi_credentials(const char* ssid, const char* pass) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, KEY_SSID, ssid);
    if (err == ESP_OK) err = nvs_set_str(handle, KEY_PASS, pass);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    return err;
}

esp_err_t load_wifi_credentials(char* ssid, char* pass) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t ssid_len = 32, pass_len = 64;
    err = nvs_get_str(handle, KEY_SSID, ssid, &ssid_len);
    if (err == ESP_OK) err = nvs_get_str(handle, KEY_PASS, pass, &pass_len);

    nvs_close(handle);
    return err;
}

esp_err_t save_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[32] = {0}, pass[64] = {0};
    // Very simplified parsing: ssid=...&pass=...
    char *p_ssid = strstr(buf, "ssid=");
    char *p_pass = strstr(buf, "&pass=");

    if (p_ssid && p_pass) {
        *p_pass = '\0'; 
        strcpy(ssid, p_ssid + 5);
        strcpy(pass, p_pass + 6);
        
        ESP_LOGI(TAG, "Saving: SSID=%s", ssid);
        save_wifi_credentials(ssid, pass);
        
        httpd_resp_send(req, "Saved. Rebooting...", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart(); // Restart to re-evaluate Main Logic
    }
    return ESP_OK;
}

esp_err_t root_get_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
}

// Renamed from start_ap_server to be the main public entry point for this mode
void wifi_start_ap_mode(void) {
    wifi_set_ap_active_bit(true);
    
    // Create AP interface
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    
    // Config Static IP
    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(ap_netif)); 
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif)); 

    IP4_ADDR(&ip_info.ip, 192, 168, 10, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 10, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    
    // Init WiFi Driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_cfg = { .ap = { .ssid = "ESP32_Config", .channel = 1, .authmode = WIFI_AUTH_OPEN, .max_connection = 1 } };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    // Start Web Server
    httpd_handle_t server = NULL;   
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_post = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler };
        httpd_register_uri_handler(server, &uri_post);
    }
    ESP_LOGI(TAG, "AP Mode Started. Connect to 'ESP32_Config' and go to 192.168.10.1");
}