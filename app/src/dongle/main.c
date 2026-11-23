#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <hal/nrf_power.h>

#include "dongle.h"
#include "hid.h"
#include "link.h"

LOG_MODULE_REGISTER(bmk_dongle, LOG_LEVEL_INF);

#define REPORT_MAX_LEN 9

typedef struct
{
    uint8_t len;
    uint8_t data[REPORT_MAX_LEN];
} usb_report_t;

enum
{
    LINK_SECURED,
    LINK_DISCOVERED,
    LINK_SUBSCRIBED,
};

K_MSGQ_DEFINE(report_msgq, sizeof(usb_report_t), USB_TX_CACHE_SIZE, 1);
K_SEM_DEFINE(usb_in_sem, 1, 1);

static const uint8_t free_mark[] = {BMK_LINK_FREE_MARK};

static const struct device *usb_hid_dev;
static atomic_t usb_suspended;

static struct bt_conn *keyboard_conn;
static atomic_t link_flags;
static struct bt_uuid_16 discover_uuid = BT_UUID_INIT_16(0);
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;

static void start_scan(void);

static void queue_report(const uint8_t *data, uint16_t len)
{
    usb_report_t report;

    if (len == 0 || len > REPORT_MAX_LEN)
    {
        return;
    }

    report.len = len;
    memcpy(report.data, data, len);

    if (k_msgq_put(&report_msgq, &report, K_NO_WAIT))
    {
        k_msgq_purge(&report_msgq);
        k_msgq_put(&report_msgq, &report, K_NO_WAIT);
    }
}

static void release_all_keys(void)
{
    static const uint8_t keyboard[9] = {BMK_HID_REPORT_ID_KEYBOARD};
    static const uint8_t consumer[7] = {BMK_HID_REPORT_ID_CONSUMER};

    queue_report(keyboard, sizeof(keyboard));
    queue_report(consumer, sizeof(consumer));
}

static void usb_int_in_ready(const struct device *dev)
{
    k_sem_give(&usb_in_sem);
}

static const struct hid_ops usb_ops = {
    .int_in_ready = usb_int_in_ready,
};

static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
    switch (status)
    {
    case USB_DC_SUSPEND:
        atomic_set(&usb_suspended, 1);
        break;
    case USB_DC_RESUME:
    case USB_DC_RESET:
    case USB_DC_CONFIGURED:
        atomic_set(&usb_suspended, 0);
        break;
    default:
        break;
    }
}

static void usb_tx_thread(void *p1, void *p2, void *p3)
{
    usb_report_t report;

    while (1)
    {
        k_msgq_get(&report_msgq, &report, K_FOREVER);

        if (atomic_get(&usb_suspended))
        {
            usb_wakeup_request();
        }

        for (int i = 0; i < USB_TX_RETRIES; i++)
        {
            k_sem_take(&usb_in_sem, K_MSEC(USB_TX_WAIT_MS));
            if (hid_int_ep_write(usb_hid_dev, report.data, report.len, NULL) == 0)
            {
                break;
            }
        }
    }
}

K_THREAD_DEFINE(usb_tx_tid, USB_TX_THREAD_STACK_SIZE, usb_tx_thread, NULL, NULL, NULL,
                K_PRIO_COOP(USB_TX_THREAD_PRIORITY), 0, 0);

static uint8_t report_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                             const void *data, uint16_t length)
{
    if (!data)
    {
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    queue_report(data, length);
    return BT_GATT_ITER_CONTINUE;
}

static void try_subscribe(struct bt_conn *conn)
{
    if (!atomic_test_bit(&link_flags, LINK_SECURED) ||
        !atomic_test_bit(&link_flags, LINK_DISCOVERED) ||
        atomic_test_and_set_bit(&link_flags, LINK_SUBSCRIBED))
    {
        return;
    }

    int err = bt_gatt_subscribe(conn, &subscribe_params);
    if (err && err != -EALREADY)
    {
        LOG_ERR("Subscribe error: %d", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return;
    }
    LOG_INF("Keyboard linked");
}

static uint8_t discover_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           struct bt_gatt_discover_params *params)
{
    int err = 0;

    if (!attr)
    {
        LOG_ERR("HID report not found");
        memset(params, 0, sizeof(*params));
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        return BT_GATT_ITER_STOP;
    }

    if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HIDS))
    {
        memcpy(&discover_uuid, BT_UUID_HIDS_REPORT, sizeof(discover_uuid));
        discover_params.uuid = &discover_uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        err = bt_gatt_discover(conn, &discover_params);
    }
    else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_HIDS_REPORT))
    {
        memcpy(&discover_uuid, BT_UUID_GATT_CCC, sizeof(discover_uuid));
        discover_params.uuid = &discover_uuid.uuid;
        discover_params.start_handle = attr->handle + 2;
        discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
        subscribe_params.value_handle = bt_gatt_attr_value_handle(attr);
        err = bt_gatt_discover(conn, &discover_params);
    }
    else
    {
        subscribe_params.notify = report_notify;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.ccc_handle = attr->handle;
        atomic_set_bit(subscribe_params.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);
        atomic_set_bit(&link_flags, LINK_DISCOVERED);
        try_subscribe(conn);
    }

    if (err)
    {
        LOG_ERR("Discover error: %d", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }

    return BT_GATT_ITER_STOP;
}

static bool find_free_mark(struct bt_data *data, void *user_data)
{
    if (data->type == BT_DATA_MANUFACTURER_DATA &&
        data->data_len == sizeof(free_mark) &&
        !memcmp(data->data, free_mark, sizeof(free_mark)))
    {
        *(bool *)user_data = true;
        return false;
    }
    return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad)
{
    bool free_keyboard = false;

    if (keyboard_conn || type != BT_GAP_ADV_TYPE_ADV_IND)
    {
        return;
    }

    bt_data_parse(ad, find_free_mark, &free_keyboard);
    if (!free_keyboard)
    {
        return;
    }

    if (bt_le_scan_stop())
    {
        return;
    }

    int err = bt_conn_le_create(
        addr,
        BT_CONN_LE_CREATE_PARAM(BT_CONN_LE_OPT_NONE, SCAN_INTERVAL, SCAN_INTERVAL),
        BT_LE_CONN_PARAM(CONN_INTERVAL, CONN_INTERVAL, 0, CONN_TIMEOUT),
        &keyboard_conn);
    if (err)
    {
        LOG_ERR("Create connection error: %d", err);
        start_scan();
    }
}

static void start_scan(void)
{
    int err = bt_le_scan_start(
        BT_LE_SCAN_PARAM(BT_LE_SCAN_TYPE_PASSIVE, BT_LE_SCAN_OPT_NONE,
                         SCAN_INTERVAL, SCAN_INTERVAL),
        device_found);
    if (err && err != -EALREADY)
    {
        LOG_ERR("Scan error: %d", err);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (conn != keyboard_conn)
    {
        return;
    }

    if (err)
    {
        LOG_ERR("Connection error: %d", err);
        bt_conn_unref(keyboard_conn);
        keyboard_conn = NULL;
        start_scan();
        return;
    }

    LOG_INF("Connected");
    bt_le_scan_stop();
    atomic_clear(&link_flags);

    memcpy(&discover_uuid, BT_UUID_HIDS, sizeof(discover_uuid));
    discover_params.uuid = &discover_uuid.uuid;
    discover_params.func = discover_cb;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int disc_err = bt_gatt_discover(conn, &discover_params);
    if (disc_err)
    {
        LOG_ERR("Discover error: %d", disc_err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (conn != keyboard_conn)
    {
        return;
    }

    LOG_INF("Disconnected reason %d", reason);
    bt_conn_unref(keyboard_conn);
    keyboard_conn = NULL;
    atomic_clear(&link_flags);
    release_all_keys();
    start_scan();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    if (err)
    {
        LOG_ERR("Security error: %d", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
        return;
    }

    if (level >= BT_SECURITY_L2)
    {
        atomic_set_bit(&link_flags, LINK_SECURED);
        try_subscribe(conn);
    }
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
    param->interval_min = CONN_INTERVAL;
    param->interval_max = CONN_INTERVAL;
    return true;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
    .le_param_req = le_param_req,
};

int main(void)
{
    int err;

#if defined(CONFIG_SOC_NRF52840_QIAA)
    nrf_power_dcdcen_set(NRF_POWER, true);
#endif

    usb_hid_dev = device_get_binding("HID_0");
    if (!usb_hid_dev)
    {
        LOG_ERR("USB HID device not found");
        return 0;
    }
    usb_hid_register_device(usb_hid_dev, hid_report_map, sizeof(hid_report_map), &usb_ops);
    usb_hid_init(usb_hid_dev);

    err = usb_enable(usb_status_cb);
    if (err)
    {
        LOG_ERR("USB enable error: %d", err);
    }

    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth enable error: %d", err);
        return 0;
    }

    LOG_INF("BMK Dongle started!");

    while (1)
    {
        if (!keyboard_conn)
        {
            start_scan();
        }
        k_msleep(SCAN_GUARD_MS);
    }

    return 0;
}

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
    sys_reboot(SYS_REBOOT_COLD);
    CODE_UNREACHABLE;
}
