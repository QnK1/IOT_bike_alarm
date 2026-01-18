#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "ble_config.h"

#define TAG "BLE_CONFIG"
#define NVS_NAMESPACE "storage"
#define KEY_USER_ID   "user_id"

#define PROFILE_NUM 1
#define PROFILE_APP_ID 0

// Custom Service UUID: 00FF
#define SERVICE_UUID 0x00FF
// Custom Char UUID: FF01 (For User ID)
#define CHAR_UUID_USER 0xFF01

static uint16_t s_handle_table[4]; 

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

bool is_user_assigned(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return false;
    
    size_t required_size;
    esp_err_t err = nvs_get_str(handle, KEY_USER_ID, NULL, &required_size);
    nvs_close(handle);
    
    return (err == ESP_OK && required_size > 0);
}

static void save_user_id(const char* user_id, size_t len) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "NVS Open Failed");
        return;
    }

    char buf[64] = {0};
    if (len > 63) len = 63;
    memcpy(buf, user_id, len);

    esp_err_t err = nvs_set_str(handle, KEY_USER_ID, buf);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "User ID Saved: %s", buf);
    } else {
        ESP_LOGE(TAG, "Failed to save User ID");
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
             if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                 ESP_LOGE(TAG, "Advertising start failed");
             } else {
                 ESP_LOGI(TAG, "BLE Advertising Started");
             }
            break;
        
        // --- Security Requests ---
        case ESP_GAP_BLE_SEC_REQ_EVT:
            /* Send the positive (true) security response to accept the request */
            // FIXED LINE BELOW:
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            ESP_LOGI(TAG, "Security Request Accepted");
            break;

        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            if (param->ble_security.auth_cmpl.success) {
                ESP_LOGI(TAG, "Pairing/Bonding Success");
            } else {
                ESP_LOGE(TAG, "Pairing Failed. Reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
            }
            break;
            
        default:
            break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            s_gatts_if = gatts_if;
            
            // --- NEW: Security Configuration ---
            /* Set the IO capabilities to No Input No Output (Just Works) */
            esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
            esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
            
            /* Set Auth Request mode to perform bonding */
            esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND; 
            esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

            esp_ble_gap_set_device_name("ESP32_SEC_CONFIG");
            esp_ble_gap_config_adv_data(&adv_data);

            esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = SERVICE_UUID;

            esp_ble_gatts_create_service(gatts_if, &service_id, 4);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            s_handle_table[0] = param->create.service_handle;
            esp_ble_gatts_start_service(s_handle_table[0]);

            esp_bt_uuid_t char_uuid;
            char_uuid.len = ESP_UUID_LEN_16;
            char_uuid.uuid.uuid16 = CHAR_UUID_USER;

            esp_attr_value_t char_val = {
                .attr_max_len = 0x40,
                .attr_len     = 0,
                .attr_value   = NULL,
            };

            esp_ble_gatts_add_char(s_handle_table[0], &char_uuid,
                                   ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
                                   ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ,
                                   &char_val, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            s_handle_table[1] = param->add_char.attr_handle;
            ESP_LOGI(TAG, "Characteristic Added");
            break;
        }
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "BLE Client Connected");
            // Optionally update connection params here if connection is unstable
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            conn_params.min_int = 0x10;
            conn_params.max_int = 0x20;
            conn_params.latency = 0;
            conn_params.timeout = 400;
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "BLE Client Disconnected");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_WRITE_EVT: {
            if (param->write.handle == s_handle_table[1]) {
                ESP_LOGI(TAG, "Writing User ID...");
                save_user_id((char*)param->write.value, param->write.len);
            }
            break;
        }
        default:
            break;
    }
}

void ble_config_init(void) {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gatts_register_callback(gatts_profile_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    
    esp_ble_gatts_app_register(PROFILE_APP_ID);
    
    ESP_LOGI(TAG, "BLE Config Mode Initialized");
}