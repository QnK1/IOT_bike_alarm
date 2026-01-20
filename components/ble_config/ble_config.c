#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "ble_config.h"
#include "nvs_store.h"

#define TAG "BLE_CONFIG"

#define PROFILE_NUM 1
#define PROFILE_APP_ID 0

// UUIDs
#define SERVICE_UUID      0x00FF
#define CHAR_UUID_USER    0xFF01 // Read/Write: User ID
#define CHAR_UUID_SSID    0xFF02 // Read/Write: WiFi SSID
#define CHAR_UUID_PASS    0xFF03 // Write Only: WiFi Password
#define CHAR_UUID_ACTION  0xFF04 // Write Only: Send '1' to Save & Reboot
#define CHAR_UUID_DEV_ID  0xFF05 // Read/Write: Device ID

// Handles index
enum {
    IDX_SVC,
    IDX_CHAR_USER,
    IDX_CHAR_VAL_USER,
    IDX_CHAR_SSID,
    IDX_CHAR_VAL_SSID,
    IDX_CHAR_PASS,
    IDX_CHAR_VAL_PASS,
    IDX_CHAR_ACT,
    IDX_CHAR_VAL_ACT,
    IDX_CHAR_DEV_ID,
    IDX_CHAR_VAL_DEV_ID,
    IDX_NB,
};

static uint16_t s_handle_table[IDX_NB];
static bool s_is_active = false;

// Temporary buffers for config
static char s_temp_ssid[32] = {0};
static char s_temp_pass[64] = {0};

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

bool ble_config_is_active(void) {
    return s_is_active;
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
            
            // --- Security Configuration ---
            // Set IO capabilities to No Input No Output (Just Works)
            esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
            esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
            
            // Set Auth Request mode to perform bonding
            esp_ble_auth_req_t auth_req = ESP_LE_AUTH_BOND; 
            esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

            // --- Set Dynamic Device Name ---
            char device_id[32] = {0};
            char ble_name[48] = "ESP32_CONFIG";
            if (nvs_load_device_id(device_id, sizeof(device_id)) == ESP_OK && strlen(device_id) > 0) {
                snprintf(ble_name, sizeof(ble_name), "BA_%s", device_id);
            }
            esp_ble_gap_set_device_name(ble_name);
            esp_ble_gap_config_adv_data(&adv_data);

            // Create Service
            esp_gatt_srvc_id_t service_id;
            service_id.is_primary = true;
            service_id.id.inst_id = 0x00;
            service_id.id.uuid.len = ESP_UUID_LEN_16;
            service_id.id.uuid.uuid.uuid16 = SERVICE_UUID;

            esp_ble_gatts_create_service(gatts_if, &service_id, IDX_NB);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            s_handle_table[IDX_SVC] = param->create.service_handle;
            esp_ble_gatts_start_service(s_handle_table[IDX_SVC]);

            // --- 1. User ID Char (Read/Write Encrypted) ---
            esp_bt_uuid_t uuid_user = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = CHAR_UUID_USER } };
            esp_ble_gatts_add_char(s_handle_table[IDX_SVC], &uuid_user,
                                   // PERMISSIONS: Require Encryption
                                   ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
                                   // PROPERTIES
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   NULL, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT: {
            uint16_t handle = param->add_char.attr_handle;
            esp_bt_uuid_t *uuid = &param->add_char.char_uuid;

            if (uuid->uuid.uuid16 == CHAR_UUID_USER) {
                s_handle_table[IDX_CHAR_VAL_USER] = handle;
                
                // --- 2. SSID Char (Read/Write Encrypted) ---
                esp_bt_uuid_t uuid_ssid = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = CHAR_UUID_SSID } };
                esp_ble_gatts_add_char(s_handle_table[IDX_SVC], &uuid_ssid,
                                       // PERMISSIONS: Require Encryption
                                       ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
                                       // PROPERTIES
                                       ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);

            } else if (uuid->uuid.uuid16 == CHAR_UUID_SSID) {
                s_handle_table[IDX_CHAR_VAL_SSID] = handle;

                // --- 3. PASS Char (Write Only, Encrypted) ---
                esp_bt_uuid_t uuid_pass = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = CHAR_UUID_PASS } };
                esp_ble_gatts_add_char(s_handle_table[IDX_SVC], &uuid_pass,
                                       // PERMISSIONS: Require Encryption
                                       ESP_GATT_PERM_WRITE_ENCRYPTED,
                                       // PROPERTIES
                                       ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);

            } else if (uuid->uuid.uuid16 == CHAR_UUID_PASS) {
                s_handle_table[IDX_CHAR_VAL_PASS] = handle;

                // --- 4. ACTION Char (Write Only, Encrypted) ---
                esp_bt_uuid_t uuid_act = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = CHAR_UUID_ACTION } };
                esp_ble_gatts_add_char(s_handle_table[IDX_SVC], &uuid_act,
                                       // PERMISSIONS: Require Encryption
                                       ESP_GATT_PERM_WRITE_ENCRYPTED,
                                       // PROPERTIES
                                       ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);
            } else if (uuid->uuid.uuid16 == CHAR_UUID_ACTION) {
                s_handle_table[IDX_CHAR_VAL_ACT] = handle;

                // --- 5. DEVICE ID Char (Read/Write Encrypted) ---
                esp_bt_uuid_t uuid_dev_id = { .len = ESP_UUID_LEN_16, .uuid = { .uuid16 = CHAR_UUID_DEV_ID } };
                esp_ble_gatts_add_char(s_handle_table[IDX_SVC], &uuid_dev_id,
                                       ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED,
                                       ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                       NULL, NULL);
            } else if (uuid->uuid.uuid16 == CHAR_UUID_DEV_ID) {
                s_handle_table[IDX_CHAR_VAL_DEV_ID] = handle;
                ESP_LOGI(TAG, "All Characteristics Added");
            }
            break;
        }
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "BLE Client Connected");
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
            if (param->write.len == 0) break;
            
            // --- USER ID ---
            if (param->write.handle == s_handle_table[IDX_CHAR_VAL_USER]) {
                char buf[64] = {0};
                int len = (param->write.len > 63) ? 63 : param->write.len;
                memcpy(buf, param->write.value, len);
                ESP_LOGI(TAG, "Setting User ID: %s", buf);
                nvs_save_user_id(buf);
            }
            // --- SSID ---
            else if (param->write.handle == s_handle_table[IDX_CHAR_VAL_SSID]) {
                int len = (param->write.len > 31) ? 31 : param->write.len;
                memset(s_temp_ssid, 0, sizeof(s_temp_ssid));
                memcpy(s_temp_ssid, param->write.value, len);
                ESP_LOGI(TAG, "Staged SSID: %s", s_temp_ssid);
            }
            // --- PASS ---
            else if (param->write.handle == s_handle_table[IDX_CHAR_VAL_PASS]) {
                int len = (param->write.len > 63) ? 63 : param->write.len;
                memset(s_temp_pass, 0, sizeof(s_temp_pass));
                memcpy(s_temp_pass, param->write.value, len);
                ESP_LOGI(TAG, "Staged Password (len: %d)", len);
            }
            // --- ACTION (Save & Reboot) ---
            else if (param->write.handle == s_handle_table[IDX_CHAR_VAL_ACT]) {
                if (param->write.value[0] == 1) {
                    ESP_LOGI(TAG, "Action received: Saving WiFi Creds & Rebooting...");
                    if (strlen(s_temp_ssid) > 0) {
                        nvs_save_wifi_creds(s_temp_ssid, s_temp_pass);
                        ESP_LOGI(TAG, "Credentials Saved.");
                    } else {
                        ESP_LOGW(TAG, "No SSID staged, skipping WiFi save.");
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
            // --- DEVICE ID ---
            else if (param->write.handle == s_handle_table[IDX_CHAR_VAL_DEV_ID]) {
                char buf[32] = {0};
                int len = (param->write.len > 31) ? 31 : param->write.len;
                memcpy(buf, param->write.value, len);
                ESP_LOGI(TAG, "Setting Device ID: %s", buf);
                nvs_save_device_id(buf);
            }
            break;
        }
        case ESP_GATTS_READ_EVT: {
            // Allow reading back the User ID or SSID (but not password)
             if (param->read.handle == s_handle_table[IDX_CHAR_VAL_USER]) {
                 char buf[64] = {0};
                 nvs_load_user_id(buf, sizeof(buf));
                 
                 esp_gatt_rsp_t rsp;
                 memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                 rsp.attr_value.handle = param->read.handle;
                 rsp.attr_value.len = strlen(buf);
                 memcpy(rsp.attr_value.value, buf, rsp.attr_value.len);
                 esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                             ESP_GATT_OK, &rsp);
             }
             else if (param->read.handle == s_handle_table[IDX_CHAR_VAL_SSID]) {
                 esp_gatt_rsp_t rsp;
                 memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                 rsp.attr_value.handle = param->read.handle;
                 rsp.attr_value.len = strlen(s_temp_ssid);
                 memcpy(rsp.attr_value.value, s_temp_ssid, rsp.attr_value.len);
                 esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                             ESP_GATT_OK, &rsp);
             }
             else if (param->read.handle == s_handle_table[IDX_CHAR_VAL_DEV_ID]) {
                 char buf[32] = {0};
                 nvs_load_device_id(buf, sizeof(buf));
                 
                 esp_gatt_rsp_t rsp;
                 memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                 rsp.attr_value.handle = param->read.handle;
                 rsp.attr_value.len = strlen(buf);
                 memcpy(rsp.attr_value.value, buf, rsp.attr_value.len);
                 esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                             ESP_GATT_OK, &rsp);
             }
            break;
        }
        default:
            break;
    }
}

void ble_config_init(void) {
    s_is_active = true;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gatts_register_callback(gatts_profile_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);
    
    esp_ble_gatts_app_register(PROFILE_APP_ID);
    
    ESP_LOGI(TAG, "BLE Config Mode Initialized (User + WiFi Provisioning) - Encryption Enabled");
}

void ble_config_deinit(void) {
    s_is_active = false;
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        return;
    }
    ESP_LOGI(TAG, "Stopping BLE...");
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    ESP_LOGI(TAG, "BLE De-initialized.");
}