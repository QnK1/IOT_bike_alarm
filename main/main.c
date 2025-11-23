#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
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
#define HID_REPORT_LEN 8
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_RSHIFT 0x20
// static const char *TARGET_DEVICE_NAME = "ESP32-Keyboard";
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

/**
 * @brief Callback wywoływany przez NimBLE po otrzymaniu powiadomienia (Notification).
 * @param conn_handle Uchwyt połączenia.
 * @param attr_handle Uchwyt atrybutu, który wysłał powiadomienie (Charakterystyka HID Report).
 * @param om Wskaźnik do bufora os_mbuf zawierającego odebrane dane.
 * @param status Status operacji (powinien być 0 przy sukcesie).
 * @param arg Opcjonalny argument (przekazany jako NULL w tym przypadku).
 * @return int Zawsze zwraca 0.
 */

static int blecent_on_notify(uint16_t conn_handle, uint16_t attr_handle,
                             struct os_mbuf *om, int status, void *arg){
    if (status != 0) {
        MODLOG_DFLT(ERROR, "Błąd powiadomienia/wskazania: status=%d\n", status);
        return 0;       // We do not disconnect because this may be a temporary error.
    }
    if (om->om_len != HID_REPORT_LEN) { // Check if the received length matches the expected length (8 bytes)
        MODLOG_DFLT(INFO, "Otrzymano powiadomienie o nieoczekiwanej długości: %d bajtów.\n", om->om_len);
        return 0;
    }
    uint8_t report[HID_REPORT_LEN]; // [Modifiers, Padding (Reserved), Key1, Key2, Key3, Key4, Key5, Key6]
    int rc = os_mbuf_copydata(om, 0, HID_REPORT_LEN, report); // 3. copying data from buffer os_mbuf to table
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Błąd kopiowania danych z os_mbuf: %d\n", rc);
        return 0;
    }
    uint8_t modifiers = report[0]; // modifiers (ctrl, shift etc.)
    // report[1] is reserved (padding)
    uint8_t key_code = report[2]; // first pressed key
    char decoded_char = hid_to_char(key_code, modifiers);
    MODLOG_DFLT(INFO, "%c", decoded_char);
    return 0;
}

static int blecent_on_subscribe(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr,
                            void *arg){
    if (error->status != 0) {
        MODLOG_DFLT(ERROR, "Błąd subskrypcji; status=%d\n", error->status);
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
    cccd = peer_dsc_find_uuid(  // find descriptor CCCD, UUID 0x2902
                peer,           // a central object storing all discovered GATT attributes for this connection
                BLE_UUID16_DECLARE(HID_SVC_UUID),
                BLE_UUID16_DECLARE(HID_REPORT_UUID),
                BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));

    if (cccd == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer lacks CCCD for HID Report characteristic\n");
        goto err;
    }

    
    rc = ble_gattc_write_flat(      // subscription
            peer->conn_handle,
            cccd->dsc.handle,       // descriptor handle
            val,
            sizeof(val),
            blecent_on_subscribe,   // callback
            (void *)report_chr);    // args for callback
            
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write CCCD; rc=%d\n", rc);
        goto err;
    }
    MODLOG_DFLT(INFO, "Subscribed to HID notifications\n");
    return;

err:
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM); // connection handle, error code
}

static void blecent_on_disc_completed(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        // Service discovery failed.  Terminate the connection.
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
        // Service discovery has completed successfully.  Now we have a complete
        // list of services, characteristics, and descriptors that the peer supports.
    MODLOG_DFLT(INFO, "Service discovery complete; status=%d conn_handle=%d\n", status, peer->conn_handle);
    blecent_subscribe(peer);
}

static void blecent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {0};
    int rc;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);       // Figure out address to use while advertising (arg 0 -> no privacy for now - RPA)
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    disc_params.filter_duplicates = 1;  // filter duplicates
    disc_params.passive = 1;
    disc_params.itvl = 0;           // interval of scan
    disc_params.window = 0;         // Scan Window - each interval listening time
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
    // uint8_t test_addr[6];
    // uint32_t peer_addr[6];
    // memset(peer_addr, 0x0, sizeof peer_addr);       // initializing the array with zeros 

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&       // Connectable Undirected Advertising -> to everyone
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {   // Connectable Directed Advertising -> to specific address
        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data); // parsing the raw data into fields
    if (rc != 0) {
        return 0;
    }

    int hid_found = 0;
    for (i = 0; i < fields.num_uuids16; i++) {      // check if any service is HID
        if (ble_uuid_u16(&fields.uuids16[i].u) == HID_SERVICE_UUID) {
            hid_found = 1;
            break;
        }
    }
    if (!hid_found) {
        return 0;
    }
    if (fields.name != NULL && fields.name_len == strlen(TARGET_DEVICE_NAME) &&     // check if name is the same
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
   
    rc = ble_gap_disc_cancel();      // Scanning must be stopped before a connection can be initiated
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &own_addr_type);       // Figure out address to use for connect ( arg 0 -> no privacy for now - RPA)
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }
    addr = &(((struct ble_gap_disc_desc *)disc)->addr);
    // Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for timeout
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
            MODLOG_DFLT(ERROR, "Failed to parse advertising data (RC=%d). Address: %s\n",
                rc, addr_str(event->disc.addr.val));
            return 0;
        }

        print_adv_fields(&fields);      // An advertisement report was received during GAP discovery
        blecent_connect_if_interesting(&(event->disc));
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            
            rc = peer_add(event->connect.conn_handle);      // Remember peer
            if (rc != 0) {  
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

//             /** Initiate security - It will perform
//              * Pairing (Exchange keys)
//              * Bonding (Store keys)
//              * Encryption (Enable encryption)
//              * Will invoke event BLE_GAP_EVENT_ENC_CHANGE
//              **/
//             rc = ble_gap_security_initiate(event->connect.conn_handle);
//             if (rc != 0) {
//                 MODLOG_DFLT(INFO, "Security could not be initiated, rc = %d\n", rc);
//                 return ble_gap_terminate(event->connect.conn_handle,
//                                          BLE_ERR_REM_USER_CONN_TERM);
//             } else {
//                 MODLOG_DFLT(INFO, "Connection secured\n");
//             }

            /* Perform service discovery */
            rc = peer_disc_all(event->connect.conn_handle, blecent_on_disc_completed, NULL);
            if(rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }

        } else {
            /* Connection attempt failed; resume scanning. */
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n", event->connect.status);
            blecent_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        MODLOG_DFLT(INFO, "disconnect; reason=%d", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        
        peer_delete(event->disconnect.conn.conn_handle);    // Forget about peer
        blecent_scan();     // Resume scanning
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:       // ble_gap_disc() completed
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:          // when encrypting was enabled, disabled or encryption attempt failed
        MODLOG_DFLT(INFO, "encryption change event; status=%d ", event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);       // check if connection exists
        assert(rc == 0);
        print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        blecent_on_notify(event->notify_rx.conn_handle,     // conn handle
                          event->notify_rx.attr_handle,     // chr handle
                          event->notify_rx.om,              // data buffer
                          0,                                // status
                          NULL);                            // args passed to callback
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link. */
        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);   
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    default:
        return 0;
    }
}

static void blecent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
    // ble_hs_sched_reset(reason);
}

static void
blecent_on_sync(void)
{
    int rc;
    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);    // arg 0 -> app does not require privacy
    assert(rc == 0);
    blecent_scan();

}

void blecent_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void
app_main(void)
{
    
    esp_err_t ret = nvs_flash_init();       // initialization and error handling of Non-Volatile Memory
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

    /* Configure the host. */
    ble_hs_cfg.reset_cb = blecent_on_reset;                     // Handle stack reset errors
    ble_hs_cfg.sync_cb = blecent_on_sync;                       // Stack ready callback
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;      // Handle key storage status

    int rc;
                                    // Initialize data structures to track connected peers.
    rc = peer_init(1, 64, 64, 64);  // args: max connections, max services, max characteristics, max descriptors
    assert(rc == 0);


    int m;
    /* Set the default device name. */
    m = ble_svc_gap_device_name_set("nimble-blecent");
    assert(m == 0);


    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(blecent_host_task);       // adds new task to run nimble host

}
