#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_rom_sys.h"

#include "http.h"
#include "wifi.h"


static const char *TAG = "HTTP";

void http_get_task(void *pvParameters)
{
    const char *WEB_SERVER = "example.com";
    const char *WEB_PORT = "80";
    const char *WEB_PATH = "/";

    char request[256];
    sprintf(request, "GET %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "User-Agent: esp-idf/5.5.1\r\n"
                    "\r\n",
                    WEB_PATH, WEB_SERVER);

    while (!wifi_is_connected())
    {
        ESP_LOGI(TAG, "Waiting for WiFi connection...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "Connecting to %s:%s", WEB_SERVER, WEB_PORT);

    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

    if (err != 0 || res == NULL)
    {
        ESP_LOGE(TAG, "Could not resolve host address: %s", WEB_SERVER);
        vTaskDelete(NULL);
        return;
    }

    int s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket");
        freeaddrinfo(res);
        vTaskDelete(NULL);
        return;
    }

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
    {
        ESP_LOGE(TAG, "Failed to connect to server");
        close(s);
        freeaddrinfo(res);
        vTaskDelete(NULL);
        return;
    }

    freeaddrinfo(res);


    if (write(s, request, strlen(request)) < 0)
    {
        ESP_LOGE(TAG, "Failed to send request");
        close(s);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Sent GET request, receiving response...");

    char recv_buf[512];
    int len;
    while ((len = read(s, recv_buf, sizeof(recv_buf) - 1)) > 0)
    {
        recv_buf[len] = 0;
        printf("%s", recv_buf);
    }

    ESP_LOGI(TAG, "Finished receiving data");
    close(s);
    vTaskDelete(NULL);
}