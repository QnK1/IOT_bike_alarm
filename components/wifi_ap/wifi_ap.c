#include <string.h>
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_ap.h"
#include "wifi.h"

static const char *TAG = "WIFI_AP";

// Stałe klucze dla NVS
#define NVS_NAMESPACE "storage"
#define KEY_SSID      "wifi_ssid"
#define KEY_PASS      "wifi_pass"

/* --- HTML STRONY WWW --- */
const char* html_page = "<html><body><h1>Konfiguracja ESP32</h1><form action='/save' method='POST'>"
                        "SSID: <input name='ssid'><br>PASS: <input name='pass' type='password'><br>"
                        "<input type='submit' value='Zapisz i Restart'></form></body></html>";

static EventGroupHandle_t wifi_ap_event_group; 
#define WIFI_AP_ACTIVE_BIT (1UL << 0)

bool wifi_is_ap_active(void) {
    if (wifi_ap_event_group == NULL) {
        return false;
    }
    EventBits_t uxBits = xEventGroupGetBits(wifi_ap_event_group);
    return (uxBits & WIFI_AP_ACTIVE_BIT) != 0;
}
void wifi_set_ap_inactive(void){
    if (wifi_ap_event_group != NULL) {
        xEventGroupClearBits(wifi_ap_event_group, WIFI_AP_ACTIVE_BIT); 
    }
}
void wifi_set_ap_active(void){
    if (wifi_ap_event_group != NULL) {
        xEventGroupSetBits(wifi_ap_event_group, WIFI_AP_ACTIVE_BIT); 
    }
}

/* --- OBSŁUGA PAMIĘCI NVS --- */       

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

/* --- HANDLERY SERWERA --- */

esp_err_t save_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[32] = {0}, pass[64] = {0};
    // Bardzo uproszczone parsowanie: ssid=...&pass=...
    char *p_ssid = strstr(buf, "ssid=");
    char *p_pass = strstr(buf, "&pass=");

    if (p_ssid && p_pass) {
        *p_pass = '\0'; 
        strcpy(ssid, p_ssid + 5);
        strcpy(pass, p_pass + 6);
        
        ESP_LOGI(TAG, "Zapisywanie: SSID=%s", ssid);
        save_wifi_credentials(ssid, pass);
        
        httpd_resp_send(req, "Zapisano. Restartuje...", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart(); // Restartujemy, aby wejść w tryb STA
    }
    return ESP_OK;
}

esp_err_t root_get_handler(httpd_req_t *req)
{
    // html_page to ten twój długi string z formularzem
    extern const char* html_page; 
    
    // Wysyłamy go do przeglądarki użytkownika
    return httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
}

void start_ap_server(void) {
    wifi_set_ap_active();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = { .ap = { .ssid = "ESP32_Config", .channel = 1, .authmode = WIFI_AUTH_OPEN, .max_connection = 2 } };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
if (httpd_start(&server, &config) == ESP_OK) {
        // Konfiguracja dla GET /
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_get_handler // przekazujesz nazwę funkcji
        };
        httpd_register_uri_handler(server, &uri_get);

        // Konfiguracja dla POST /save (tę funkcję powinieneś już mieć)
        httpd_uri_t uri_post = {
            .uri      = "/save",
            .method   = HTTP_POST,
            .handler  = save_post_handler 
        };
        httpd_register_uri_handler(server, &uri_post);
    }
}

void wifi_ap_init(void) {
    if (wifi_ap_event_group == NULL) {
        wifi_ap_event_group = xEventGroupCreate();
    }
    char ssid[32] = {0}, pass[64] = {0};
    
    if (load_wifi_credentials(ssid, pass) == ESP_OK) {
        wifi_set_ap_inactive();
        ESP_LOGI(TAG, "Znaleziono dane w NVS. Lacze z %s...", ssid);
        // Tutaj wywołaj swoją poprzednią funkcję wifi_init_sta() przekazując jej ssid i pass
    } else {
        ESP_LOGW(TAG, "Brak danych w NVS. Startuje tryb konfiguracji (AP)...");

        start_ap_server();
    }
}