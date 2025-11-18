#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/dis/ble_svc_dis.h"
#include "services/bas/ble_svc_bas.h"
#include "store/config/ble_store_config.h"

#define TAG "HID_KEYBOARD"


void ble_store_config_init(void);


#define HID_SERVICE_UUID           0x1812
#define HID_INFO_UUID              0x2A4A
#define HID_REPORT_MAP_UUID        0x2A4B
#define HID_CONTROL_POINT_UUID     0x2A4C
#define HID_REPORT_UUID            0x2A4D
#define HID_PROTOCOL_MODE_UUID     0x2A4E


#define KEY_H 0x0B
#define KEY_E 0x08
#define KEY_L 0x0F
#define KEY_O 0x12
#define KEY_ENTER 0x28

static uint16_t hid_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool is_connected = false;
static uint16_t hid_report_handle;
static uint8_t own_addr_type;


static const uint8_t hid_report_map[] = {
    0x05, 0x01, 
    0x09, 0x06, 
    0xA1, 0x01, 
    0x05, 0x07, 
    0x19, 0xE0, 
    0x29, 0xE7, 
    0x15, 0x00, 
    0x25, 0x01, 
    0x75, 0x01, 
    0x95, 0x08, 
    0x81, 0x02, 
    0x95, 0x01, 
    0x75, 0x08, 
    0x81, 0x03, 
    0x95, 0x05, 
    0x75, 0x01, 
    0x05, 0x08, 
    0x19, 0x01, 
    0x29, 0x05, 
    0x91, 0x02, 
    0x95, 0x01, 
    0x75, 0x03, 
    0x91, 0x03, 
    0x95, 0x06, 
    0x75, 0x08, 
    0x15, 0x00, 
    0x25, 0x65, 
    0x05, 0x07, 
    0x19, 0x00, 
    0x29, 0x65, 
    0x81, 0x00, 
    0xC0        
};

static const uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x01};

static int hid_report_map_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, hid_report_map, sizeof(hid_report_map));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int hid_info_cb(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, hidInfo, sizeof(hidInfo));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int hid_report_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}




static const uint8_t hid_report_ref[] = {0x00, 0x01};

static int hid_report_ref_cb(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, hid_report_ref, sizeof(hid_report_ref));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(HID_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                
                .uuid = BLE_UUID16_DECLARE(HID_INFO_UUID),
                .access_cb = hid_info_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                
                .uuid = BLE_UUID16_DECLARE(HID_REPORT_MAP_UUID),
                .access_cb = hid_report_map_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                
                
                .uuid = BLE_UUID16_DECLARE(HID_REPORT_UUID),
                .access_cb = hid_report_cb,
                .val_handle = &hid_report_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC,
                
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(0x2908), 
                        .access_cb = hid_report_ref_cb,
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC, 
                    },
                    {0} 
                },
            },
            {
                
                .uuid = BLE_UUID16_DECLARE(HID_PROTOCOL_MODE_UUID),
                .access_cb = hid_report_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                
                .uuid = BLE_UUID16_DECLARE(HID_CONTROL_POINT_UUID),
                .access_cb = hid_report_cb,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {0} 
        }
    },
    {0} 
};

void send_key(uint8_t key) {
    if (!is_connected) return;
    
    
    uint8_t report[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    report[2] = key; 

    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(report));
    
    
    
    int rc = ble_gatts_notify_custom(hid_conn_handle, hid_report_handle, om);
    
    if (rc != 0) {
        ESP_LOGE(TAG, "Error sending key press: %d", rc);
    }

    vTaskDelay(pdMS_TO_TICKS(20)); 

    
    report[2] = 0x00;
    om = ble_hs_mbuf_from_flat(report, sizeof(report));
    rc = ble_gatts_notify_custom(hid_conn_handle, hid_report_handle, om);
}

void type_task(void *pvParameters) {
    while (1) {
        if (is_connected) {
            ESP_LOGI(TAG, "Typing 'hello'...");
            send_key(KEY_H); vTaskDelay(pdMS_TO_TICKS(50));
            send_key(KEY_E); vTaskDelay(pdMS_TO_TICKS(50));
            send_key(KEY_L); vTaskDelay(pdMS_TO_TICKS(50));
            send_key(KEY_L); vTaskDelay(pdMS_TO_TICKS(50));
            send_key(KEY_O); vTaskDelay(pdMS_TO_TICKS(50));
            send_key(KEY_ENTER);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connected");
            if (event->connect.status == 0) {
                hid_conn_handle = event->connect.conn_handle;
                is_connected = true;

                
                
                
                struct ble_gap_conn_desc desc;
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc == 0) {
                     
                     
                     ble_gap_security_initiate(event->connect.conn_handle);
                }
            } else {
                
                ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, NULL, gap_event, NULL);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected. Reason: 0x%x", event->disconnect.reason);
            is_connected = false;
            hid_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            
            ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, NULL, gap_event, NULL);
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            
            ESP_LOGI(TAG, "Connection parameters updated");
            break;

        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            
            
            
            ESP_LOGI(TAG, "Connection update request received - Accepting");
            return 0; 

        case BLE_GAP_EVENT_ENC_CHANGE:
            
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Security Enabled (Bonding Complete)");
            } else {
                ESP_LOGE(TAG, "Security Failed (Status: 0x%x)", event->enc_change.status);
            }
            break;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            
            
            
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            if (rc == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    return 0;
}

void ble_app_on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }

    
    
    ble_svc_gap_device_appearance_set(0x03C1);
    
    
    ble_svc_gap_device_name_set("ESP32-NimBLE-Key");

    
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = 1;
    ble_hs_cfg.sm_their_key_dist = 1;

    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"ESP32-NimBLE-Key";
    fields.name_len = strlen("ESP32-NimBLE-Key");
    fields.name_is_complete = 1;
    fields.appearance = 0x03C1;
    fields.appearance_is_present = 1;
    
    fields.uuids16 = (ble_uuid16_t[]) { BLE_UUID16_INIT(HID_SERVICE_UUID) };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    ble_gap_adv_set_fields(&fields);
    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    
    ESP_LOGI(TAG, "Advertising started...");
}

void ble_host_task(void *param) {
    
    nimble_port_run(); 
    
    
    nimble_port_freertos_deinit();
}

void app_main(void) {
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    
    
    
    
    
    
    
    

    
    
    
    
    

    
    
    
    
    
    

    
    nimble_port_init();

    
    ble_svc_gap_device_name_set("ESP32-NimBLE-Key");
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_dis_init();
    ble_svc_bas_init();
    
    
    
    ble_store_config_init();

    
    ble_gatts_count_cfg(svcs);
    ble_gatts_add_svcs(svcs);

    
    ble_gatts_start();
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    
    
    
    nimble_port_freertos_init(ble_host_task);

    
    xTaskCreate(type_task, "type_task", 4096, NULL, 5, NULL);
}