#ifndef H_BLECENT_
#define H_BLECENT_

#include "modlog/modlog.h"
#include "esp_central.h"
#ifdef __cplusplus
extern "C" {
#endif

struct ble_hs_adv_fields;
struct ble_gap_conn_desc;
struct ble_hs_cfg;
union ble_store_value;
union ble_store_key;

#define BLECENT_SVC_ALERT_UUID              0x1811
#define BLECENT_CHR_SUP_NEW_ALERT_CAT_UUID  0x2A47
#define BLECENT_CHR_NEW_ALERT               0x2A46
#define BLECENT_CHR_SUP_UNR_ALERT_CAT_UUID  0x2A48
#define BLECENT_CHR_UNR_ALERT_STAT_UUID     0x2A45
#define BLECENT_CHR_ALERT_NOT_CTRL_PT       0x2A44

#ifdef __cplusplus
}
#endif

#endif
