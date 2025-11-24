#include <stdio.h>
#include <string.h>
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
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_rom_sys.h"

#include "wifi.h"
#include "http.h"
#include "mqtt_cl.h"

#define TAG "HID_KEYBOARD"

void ble_store_config_init(void);

// HID service
#define HID_SERVICE_UUID           0x1812 // standard UUID of the HID service

// HID service characteristics
#define HID_INFO_UUID              0x2A4A // R, HID version etc.
#define HID_REPORT_MAP_UUID        0x2A4B // R, defines sent data structure
#define HID_CONTROL_POINT_UUID     0x2A4C // W, suspend/exit suspend
#define HID_REPORT_UUID            0x2A4D // RNW, data
#define HID_PROTOCOL_MODE_UUID     0x2A4E // RW, protocol mode (standard with report map or simplified for BIOS)

// keycodes
#define KEY_A       0x04
#define KEY_B       0x05
#define KEY_C       0x06
#define KEY_D       0x07
#define KEY_E       0x08
#define KEY_F       0x09
#define KEY_G       0x0A
#define KEY_H       0x0B
#define KEY_I       0x0C
#define KEY_J       0x0D
#define KEY_K       0x0E
#define KEY_L       0x0F
#define KEY_M       0x10
#define KEY_N       0x11
#define KEY_O       0x12
#define KEY_P       0x13
#define KEY_Q       0x14
#define KEY_R       0x15
#define KEY_S       0x16
#define KEY_T       0x17
#define KEY_U       0x18
#define KEY_V       0x19
#define KEY_W       0x1A
#define KEY_X       0x1B
#define KEY_Y       0x1C
#define KEY_Z       0x1D
#define KEY_1       0x1E
#define KEY_2       0x1F
#define KEY_3       0x20
#define KEY_4       0x21
#define KEY_5       0x22
#define KEY_6       0x23
#define KEY_7       0x24
#define KEY_8       0x25
#define KEY_9       0x26
#define KEY_0       0x27
#define KEY_ENTER        0x28
#define KEY_ESC          0x29
#define KEY_BACKSPACE    0x2A
#define KEY_TAB          0x2B
#define KEY_SPACE        0x2C
#define KEY_MINUS        0x2D  // - and _
#define KEY_EQUAL        0x2E  // = and +
#define KEY_LEFT_BRACE   0x2F  // [ and {
#define KEY_RIGHT_BRACE  0x30  // ] and }
#define KEY_BACKSLASH    0x31  // \ and |
#define KEY_NON_US_NUM   0x32  // Non-US # and ~
#define KEY_SEMICOLON    0x33  // ; and :
#define KEY_QUOTE        0x34  // ' and "
#define KEY_GRAVE        0x35  // ` and ~
#define KEY_COMMA        0x36  // , and <
#define KEY_DOT          0x37  // . and >
#define KEY_SLASH        0x38  // / and ?
#define KEY_CAPS_LOCK    0x39
#define KEY_F1      0x3A
#define KEY_F2      0x3B
#define KEY_F3      0x3C
#define KEY_F4      0x3D
#define KEY_F5      0x3E
#define KEY_F6      0x3F
#define KEY_F7      0x40
#define KEY_F8      0x41
#define KEY_F9      0x42
#define KEY_F10     0x43
#define KEY_F11     0x44
#define KEY_F12     0x45
#define KEY_PRINTSCREEN 0x46
#define KEY_SCROLL_LOCK 0x47
#define KEY_PAUSE       0x48
#define KEY_INSERT      0x49
#define KEY_HOME        0x4A
#define KEY_PAGE_UP     0x4B
#define KEY_DELETE      0x4C
#define KEY_END         0x4D
#define KEY_PAGE_DOWN   0x4E
#define KEY_RIGHT   0x4F
#define KEY_LEFT    0x50
#define KEY_DOWN    0x51
#define KEY_UP      0x52
#define KEY_MOD_LCTRL   0x01
#define KEY_MOD_LSHIFT  0x02
#define KEY_MOD_LALT    0x04
#define KEY_MOD_LMETA   0x08 // Windows Key / Command Key
#define KEY_MOD_RCTRL   0x10
#define KEY_MOD_RSHIFT  0x20
#define KEY_MOD_RALT    0x40
#define KEY_MOD_RMETA   0x80

static uint16_t hid_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool is_connected = false;
static uint16_t hid_report_handle;
static uint8_t own_addr_type;

// standard USB HID keyboard report descriptor
static const uint8_t hid_report_map[] = {
    // --- Global Device Definition ---
    0x05, 0x01, // usage page: generic desktop controls (mouse, keyboard, joystick)
    0x09, 0x06, // usage: specifically a keyboard
    0xA1, 0x01, // start of collection: everything inside here belongs to the keyboard
    
    // --- Byte 0: Modifier Keys (Input) ---
    0x05, 0x07, // usage page: switch to keyboard/keypad definitions
    0x19, 0xE0, // usage min: start at key 224 (left control)
    0x29, 0xE7, // usage max: end at key 231 (right gui/windows)
    0x15, 0x00, // logical min: key up (0)
    0x25, 0x01, // logical max: key down (1)
    0x75, 0x01, // report size: each key takes 1 bit
    0x95, 0x08, // report count: there are 8 bits total (1 byte)
    0x81, 0x02, // input: variable data (flags) -> maps to report[0] in your code

    // --- Byte 1: Reserved Padding (Input) ---
    0x95, 0x01, // report count: 1 item
    0x75, 0x08, // report size: 8 bits long
    0x81, 0x03, // input: constant/padding (computer ignores this) -> maps to report[1]

    // --- LED Status (Output from PC) ---
    0x95, 0x05, // report count: 5 items
    0x75, 0x01, // report size: 1 bit each
    0x05, 0x08, // usage page: switch to LED definitions
    0x19, 0x01, // usage min: num lock
    0x29, 0x05, // usage max: kana
    0x91, 0x02, // output: variable data (pc writes this to esp32 to turn on lights)
    
    // --- LED Padding (Output from PC) ---
    0x95, 0x01, // report count: 1 item
    0x75, 0x03, // report size: 3 bits (padding to finish the byte: 5+3=8)
    0x91, 0x03, // output: constant/padding

    // --- Bytes 2-7: The Key Array (Input) ---
    0x95, 0x06, // report count: 6 items (allows 6 keys pressed at once)
    0x75, 0x08, // report size: 8 bits (1 byte) per key
    0x15, 0x00, // logical min: key code 0
    0x25, 0x65, // logical max: key code 101 (keyboard application)
    0x05, 0x07, // usage page: back to keyboard definitions
    0x19, 0x00, // usage min: key code 0 (reserved/no event)
    0x29, 0x65, // usage max: key code 101
    0x81, 0x00, // input: array data (list of indices) -> maps to report[2] through report[7]
    
    0xC0        // end of collection
};

static const uint8_t hidInfo[] = {0x11, 0x01, 0x00, 0x01};

// callback to get the report map
static int hid_report_map_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, hid_report_map, sizeof(hid_report_map));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// callback to get the hid info
static int hid_info_cb(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, hidInfo, sizeof(hidInfo));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// callback from reads from the hid report, also used as a placeholder for protocol mode and control point
static int hid_report_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

// report reference descriptor balue: [Report ID, Report Type]
// report ID: 0x00
// report Type: 0x01 (Input Report)
static const uint8_t hid_report_ref[] = {0x00, 0x01};

static int hid_report_ref_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = os_mbuf_append(ctxt->om, hid_report_ref, sizeof(hid_report_ref));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static const struct ble_gatt_svc_def svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(HID_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // HID Information
                // R, HID version etc.
                .uuid = BLE_UUID16_DECLARE(HID_INFO_UUID),
                .access_cb = hid_info_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC, // only for encrypted reads
            },
            {
                // report map
                // R, defines sent data structure
                .uuid = BLE_UUID16_DECLARE(HID_REPORT_MAP_UUID),
                .access_cb = hid_report_map_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC,
            },
            {
                // input report (keyboard data)
                .uuid = BLE_UUID16_DECLARE(HID_REPORT_UUID),
                .access_cb = hid_report_cb,
                .val_handle = &hid_report_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ_ENC,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(0x2908), // UUID for report reference
                        .access_cb = hid_report_ref_cb,
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC, 
                    },
                    {0} // descriptor terminator
                },
            },
            {
                // protocol mode
                // RW, protocol mode (standard with report map or simplified for BIOS)
                .uuid = BLE_UUID16_DECLARE(HID_PROTOCOL_MODE_UUID),
                .access_cb = hid_report_cb,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            },
            {
                // control point
                // W, suspend/exit suspend
                .uuid = BLE_UUID16_DECLARE(HID_CONTROL_POINT_UUID),
                .access_cb = hid_report_cb,
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {0} // characteristic terminator
        }
    },
    {0} // Service terminator
};

void send_key(uint8_t key, uint8_t modifiers) {
    if (!is_connected) return;
    
    uint8_t report[8] = {0}; // init to all zeros
    
    report[0] = modifiers;   // set modifiers (shift, ctrl, etc.)
    report[2] = key;         // set keycode
    // potentially, report[3] to report[7] could be set for multiple keys pressed at once

    // convert array to nimble linked-list-like structure
    struct os_mbuf *om = ble_hs_mbuf_from_flat(report, sizeof(report));

    // send the data
    ble_gatts_notify_custom(hid_conn_handle, hid_report_handle, om);
    
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // release the key (send all zeros)
    memset(report, 0, sizeof(report));
    om = ble_hs_mbuf_from_flat(report, sizeof(report));
    ble_gatts_notify_custom(hid_conn_handle, hid_report_handle, om);
}

void type_task(void *pvParameters) {
    while (1) {
        if (is_connected) {
            ESP_LOGI(TAG, "Sending the alphabet in lowercase...");
            for (uint8_t key = 0x04; key <= 0x1D; key++) {
                send_key(key, 0); 
                vTaskDelay(pdMS_TO_TICKS(100)); 
            }

            ESP_LOGI(TAG, "Sending the alphabet in uppercase...");
            for (uint8_t key = 0x04; key <= 0x1D; key++) {
                send_key(key, KEY_MOD_LSHIFT); 
                vTaskDelay(pdMS_TO_TICKS(100)); 
            }

        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    struct ble_gap_conn_desc desc; // connection description variable
    int rc; // return code

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connected");

            // check if the connection hasn't failed immediately
            if (event->connect.status == 0) {
                hid_conn_handle = event->connect.conn_handle; // connection id
                is_connected = true;

                // get a full description of the connection with given id
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                // if return code == 0
                if (rc == 0) {
                     // if we are already bonded, encryption starts automatically
                     // if not, we request pairing here
                    ble_gap_security_initiate(event->connect.conn_handle);
                }
            } else {
                // connection failed; resume advertising
                ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, NULL, gap_event, NULL);
            }
            break;


        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected. Reason: 0x%x", event->disconnect.reason);
            is_connected = false;
            hid_conn_handle = BLE_HS_CONN_HANDLE_NONE; // reset the connection handle

            // start advertising again
            ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, NULL, gap_event, NULL);
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "Connection parameters updated");
            break;

        case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            ESP_LOGI(TAG, "Connection update request received - Accepting");

            // 0 has to be returned to accept the speed change required by the connected device
            // otherwise Windows would think the device is unresponsive
            return 0; 

        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status == 0) {
                ESP_LOGI(TAG, "Security Enabled (Bonding Complete)");
            } else {
                ESP_LOGE(TAG, "Security Failed (Status: 0x%x)", event->enc_change.status);
            }
            break;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            // if the devices are already paired, but pairing is requested again
            // this can happen if the device somehow gets marked as disconnected by the other device,
            // but it still thinks it is connected 
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            if (rc == 0) {
                ble_store_util_delete_peer(&desc.peer_id_addr);
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    return 0;
}

void ble_app_on_sync(void) {
    
    // determine the MAC address to use (privacy disabled in this case)
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }

    // set GAP device name and type (for after the connection is established)
    ble_svc_gap_device_appearance_set(0x03C1);
    ble_svc_gap_device_name_set("ESP32-Keyboard");

    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO; //pairing without pin codes
    ble_hs_cfg.sm_bonding = 1; // bonding, to store keys in NVS, otherwise the devices would have to be re-paired every time
    ble_hs_cfg.sm_mitm = 1; // secure channel
    ble_hs_cfg.sm_sc = 1; // newer encryption
    ble_hs_cfg.sm_our_key_dist = 1; // enc key distribution
    ble_hs_cfg.sm_their_key_dist = 1; // enc key distribution

    // safe initialization of advertising params and fields
    struct ble_gap_adv_params adv_params = {0};
    struct ble_hs_adv_fields fields = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connection to any device
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // visibility for discovery

    // advertisement options
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP; // indefinite visibility, BLE support only
    fields.name = (uint8_t *)"ESP32-Keyboard"; // device name (for the advertising phase)
    fields.name_len = strlen("ESP32-Keyboard"); //
    fields.name_is_complete = 1; // complete name, not an abbreviation
    fields.appearance = 0x03C1; // appear as a HID keyboard
    fields.appearance_is_present = 1; // flag to include the appearance value
    
    fields.uuids16 = (ble_uuid16_t[]) { BLE_UUID16_INIT(HID_SERVICE_UUID) }; // advertise as a HID supporting device
    fields.num_uuids16 = 1; // number of uuids passed
    fields.uuids16_is_complete = 1; // the list of uuids is exhaustive, no other services are supported

    // convert the 'fields' struct into a byte array
    ble_gap_adv_set_fields(&fields);
    // start advertising
    // non-private address, undirected advertising (to everyone), advertising timeout (forever), default settings (interval etc.), connection handler, data for the callback (NULL)
    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event, NULL);
    
    ESP_LOGI(TAG, "Advertising started...");
}

void ble_host_task(void *param) {
    // processes ble signals and triggers callbacks (infinite loop)
    nimble_port_run(); 
    
    // this line is only reached if the stack stops (rare)
    nimble_port_freertos_deinit();
}

void app_main(void) {
    // initialize NVS for persistent BLE keys storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
    mqtt_app_start();

    // allocate RAM for nimble and set up an interface between nimble and the physical layer
    nimble_port_init();

    // 5. Initialize Application Specific Services
    ble_svc_gap_device_name_set("ESP32-Keyboard");
    ble_svc_gap_init(); // device name and icon
    ble_svc_gatt_init(); // data
    ble_svc_dis_init(); // device information
    ble_svc_bas_init(); // battery information
    
    // connects nimble to nvs and looks for keys 
    ble_store_config_init(); 

    
    ble_gatts_count_cfg(svcs); // calculate how much RAM to allocate for the services
    ble_gatts_add_svcs(svcs); // register the defined services

    // mark gatt as initialized
    ble_gatts_start();
    ble_hs_cfg.sync_cb = ble_app_on_sync; // callback for starting advertising, called when ble hardware is ready
    
    // start a task that processes ble signals and triggers callbacks
    nimble_port_freertos_init(ble_host_task);

    // start the typing task
    xTaskCreate(type_task, "type_task", 4096, NULL, 5, NULL);
}