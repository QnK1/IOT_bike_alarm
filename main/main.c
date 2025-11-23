#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "nimble/nimble_opt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "blecent.h"

#define HID_SERVICE_UUID 0x1812
const uint16_t HID_SVC_UUID = 0x1812;
const uint16_t HID_REPORT_UUID = 0x2A4D;
const uint16_t GATT_UUID_BATTERY_SERVICE = 0x180f;
const uint16_t GATT_UUID_BATTERY_LEVEL = 0x2a19;
#define HID_REPORT_LEN 8
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_RSHIFT 0x20

// <--- CHANGED: Set to SilverMonkey, but check your logs!
static const char *TARGET_DEVICE_NAME = "k90m BT5.0"; 

static const char *tag = "NimBLE_BLE_CENT";
static int blecent_gap_event(struct ble_gap_event *event, void *arg);

void ble_store_config_init(void);

char hid_to_char(uint8_t keycode, uint8_t modifiers) { // letter keycode
    bool is_shift_pressed = (modifiers & KEY_MOD_LSHIFT) || (modifiers & KEY_MOD_RSHIFT);
    if (keycode >= 0x04 && keycode <= 0x1D) {
        uint8_t offset = keycode - 0x04;    // 0x04 ('A') -> 0, 0x1D ('Z') -> 25
        return is_shift_pressed ? ('A' + offset) : ('a' + offset);
    }
    else if (keycode >= 0x1E && keycode <= 0x27) { // number keycode
        uint8_t offset = keycode - 0x1E;    // 0x1E ('1') -> 0, 0x27 ('0') -> 9
        static const char number_map[] = "1234567890";
        static const char shift_map[] = "!@#$%^&*()";
        return is_shift_pressed ? shift_map[offset] : number_map[offset];
    }
    else{
        switch (keycode) {      // special char keycode
            case 0x2C: return ' ';           // KEY_SPACE
            case 0x28: return '\n';          // KEY_ENTER
            case 0x2A: return '\b';          // KEY_BACKSPACE
            case 0x2B: return '\t';          // KEY_TAB
            case 0x2D: return is_shift_pressed ? '_' : '-'; // KEY_MINUS
            case 0x2E: return is_shift_pressed ? '+' : '='; // KEY_EQUAL
        }
    }
    return '\0'; // unsupported keycode
}

static int blecent_on_notify(uint16_t conn_handle, uint16_t attr_handle,
                             struct os_mbuf *om, int status, void *arg){
    if (status != 0) {
        MODLOG_DFLT(ERROR, "Notification error: status=%d\n", status);
        return 0;
    }

    // Copy data to a local buffer to inspect it safely
    uint8_t data[32]; // Max buffer size
    uint16_t len = OS_MBUF_PKTLEN(om);      // get length of data
    if (len > sizeof(data)) len = sizeof(data);
    
    int rc = os_mbuf_copydata(om, 0, len, data);    // args: src, offset (bytes to skip), len, dst
    if (rc != 0) return 0;

    // Handle "Report ID" (Byte 0 is ID, Byte 1 is Modifier...)
    // Many commercial keyboards send 9 bytes: [ID] [Mod] [Res] [Key1]...

    uint8_t modifiers = 0;
    uint8_t key_code = 0;

    if (len >= 9) { 
        // Assume Byte 0 is Report ID, so we shift index by 1
        modifiers = data[1];
        key_code = data[3]; 
    } else if (len == 8) {
        // Standard Boot Protocol: [Mod] [Res] [Key1]...
        modifiers = data[0];
        key_code = data[2];
    } else {
        // Likely a media key (volume) or battery report
        return 0; 
    }

    // Print decoded character
    if (key_code != 0) {
        char decoded_char = hid_to_char(key_code, modifiers);
        if (decoded_char != '\0') {
            MODLOG_DFLT(INFO, "%c", decoded_char);
        }
    }

    return 0;
}

static int blecent_on_subscribe(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr,
                            void *arg){
    if (error->status != 0) {
        MODLOG_DFLT(ERROR, "Subscription failed; status=%d\n", error->status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return error->status;
    }
    MODLOG_DFLT(INFO, "Subscription completed successfully.\n");
    return 0;
}

static void blecent_subscribe(const struct peer *peer){
    const struct peer_chr *report_chr;
    const struct peer_dsc *cccd;
    int rc;
    uint8_t val[2] = {0x01, 0x00}; // enable notifications
    report_chr = peer_chr_find_uuid(
                    peer,
                    BLE_UUID16_DECLARE(HID_SVC_UUID),
                    BLE_UUID16_DECLARE(HID_REPORT_UUID));
    if (report_chr == NULL) {
        MODLOG_DFLT(ERROR,"Error: Peer does not have HID Report characteristic\n");
        goto err;
    }
    cccd = peer_dsc_find_uuid(  
                peer,           
                BLE_UUID16_DECLARE(HID_SVC_UUID),
                BLE_UUID16_DECLARE(HID_REPORT_UUID),
                BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));

    if (cccd == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer lacks CCCD for HID Report characteristic\n");
        goto err;
    }

    rc = ble_gattc_write_flat(      
            peer->conn_handle,
            cccd->dsc.handle,       
            val,
            sizeof(val),
            blecent_on_subscribe,   
            (void *)report_chr);        // arg passed to callback
            
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write CCCD; rc=%d\n", rc);
        goto err;
    }
    MODLOG_DFLT(INFO, "Subscribing to HID notifications...\n");
    return;

err:
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM); 
}

static int blecent_on_battery_read(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr, void *arg)
{
    if (error->status != 0) {
        MODLOG_DFLT(ERROR, "Battery level read FAILED; status=%d\n", error->status);
        return 0;
    }

    if (attr->om->om_len == 1) {
        uint8_t battery_level = *(uint8_t *)attr->om->om_data;  // getting battery level, 1 byte of data
        
        MODLOG_DFLT(INFO, "BATTERY LEVEL: %d\n", battery_level);
    } else {
        MODLOG_DFLT(WARN, "Received unexpected data length (%d bytes) for Battery Level (expected 1 byte).\n", attr->om->om_len);
    }

    return 0;
}
static void check_battery(const struct peer *peer)
{
    const struct peer_chr *chr;
    int rc;

    chr = peer_chr_find_uuid(peer,      // finding characteristic battery level
                             BLE_UUID16_DECLARE(GATT_UUID_BATTERY_SERVICE),
                             BLE_UUID16_DECLARE(GATT_UUID_BATTERY_LEVEL));
    
    if (chr == NULL) {
        MODLOG_DFLT(WARN, "Battery Level characteristic (0x2A19) not found.\n");
        return;
    }
    
    rc = ble_gattc_read(peer->conn_handle, chr->chr.val_handle,   // init characteristic read
                        blecent_on_battery_read, NULL);
    
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Failed to initiate battery read; rc=%d\n", rc);
    } else {
        MODLOG_DFLT(INFO, "Battery level read initiated.\n");
    }
}

static void blecent_on_disc_completed(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
    MODLOG_DFLT(INFO, "Service discovery complete; status=%d conn_handle=%d\n", status, peer->conn_handle);
    check_battery(peer);
    blecent_subscribe(peer);
}

static void blecent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {0};
    int rc;
    rc = ble_hs_id_infer_auto(0, &own_addr_type); 
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    disc_params.filter_duplicates = 1; 
    disc_params.passive = 1;
    disc_params.itvl = 0;           
    disc_params.window = 0;         
    disc_params.filter_policy = 0;  
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n", rc);
    }
}

static int blecent_should_connect(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;
    int i;

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&      
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {   
        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data); 
    if (rc != 0) {
        return 0;
    }


    int hid_found = 0;
    for (i = 0; i < fields.num_uuids16; i++) {      
        if (ble_uuid_u16(&fields.uuids16[i].u) == HID_SERVICE_UUID) {
            hid_found = 1;
            break;
        }
    }
    if (!hid_found) {
        return 0;
    }
    
    // Check if name is the same
    if (fields.name != NULL && fields.name_len == strlen(TARGET_DEVICE_NAME) &&     
        memcmp(fields.name, TARGET_DEVICE_NAME, strlen(TARGET_DEVICE_NAME)) == 0) {
        return 1; // name is the same
    }

    return 0;
}

static void
blecent_connect_if_interesting(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    if (!blecent_should_connect((struct ble_gap_disc_desc *)disc)) {
        return;
    }
   
    rc = ble_gap_disc_cancel();      
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &own_addr_type);      
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }
    addr = &(((struct ble_gap_disc_desc *)disc)->addr);
    
    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL, blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d addr=%s; rc=%d\n",
                    addr->type, addr_str(addr->val), rc);
        return;
    }
}

static int
blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            return 0;
        }
        blecent_connect_if_interesting(&(event->disc));
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            MODLOG_DFLT(INFO, "Connection established\n");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            
            rc = peer_add(event->connect.conn_handle);      
            if (rc != 0) {  
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

            // Security must be initiated for commercial keyboards
            MODLOG_DFLT(INFO, "Initiating Security/Pairing...\n");
            rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(INFO, "Security could not be initiated, rc = %d\n", rc);
                // We do NOT disconnect here immediately, some devices init security themselves
            } 

            /* Perform service discovery */
            rc = peer_disc_all(event->connect.conn_handle, blecent_on_disc_completed, NULL);
            if(rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }

        } else {
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n", event->connect.status);
            blecent_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "Disconnect; reason=%d\n", event->disconnect.reason);
        peer_delete(event->disconnect.conn.conn_handle);    
        blecent_scan();     
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:       
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:          
        MODLOG_DFLT(INFO, "Encryption change event; status=%d\n", event->enc_change.status);
        if (event->enc_change.status == 0) {
             MODLOG_DFLT(INFO, "Connection Secured (Encrypted)\n");
        }
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        blecent_on_notify(event->notify_rx.conn_handle,     
                          event->notify_rx.attr_handle,     
                          event->notify_rx.om,              
                          0,                                
                          NULL);                            
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);   
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    default:
        return 0;
    }
}

static void blecent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
blecent_on_sync(void)
{
    int rc;
    rc = ble_hs_util_ensure_addr(0);    
    assert(rc == 0);
    blecent_scan();
}

void blecent_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void
app_main(void)
{
    esp_err_t ret = nvs_flash_init();       
    if  (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    ble_hs_cfg.reset_cb = blecent_on_reset;                 
    ble_hs_cfg.sync_cb = blecent_on_sync;                   
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;  

    // This is required for commercial keyboards to trust the ESP32
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO; // No screen, no keyboard on ESP32
    ble_hs_cfg.sm_bonding = 1;                  // Enable bonding (saving keys)
                                                // after connection keys will be stored in nvm
    ble_hs_cfg.sm_mitm = 1;                     // Man-in-the-middle protection
    ble_hs_cfg.sm_sc = 1;                       // Secure Connections
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    int rc;
    rc = peer_init(1, 64, 64, 64); 
    assert(rc == 0);

    int m;
    m = ble_svc_gap_device_name_set("ESP32-Central");
    assert(m == 0);

    ble_store_config_init();

    nimble_port_freertos_init(blecent_host_task);       
}