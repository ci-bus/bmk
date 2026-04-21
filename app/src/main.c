/*
 * BMK Keyboard Firmware
 * USB HID + BLE HID with Boot Protocol
 * Priority: USB when connected, BLE otherwise
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <hal/nrf_power.h>

#include "main.h"

LOG_MODULE_REGISTER(bmk, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(send_thread_area, SEND_THREAD_STACK_SIZE);
struct k_thread send_thread_data;

K_MSGQ_DEFINE(report_msgq, sizeof(thread_report_t), SEND_THREAD_CACHE_SIZE, 4);

static struct key keys[MATRIX_COLS * MATRIX_ROWS] = {0};
#ifdef ENCODERS
static struct encoder_key encoder_keys[ENCODERS] = {0};
#endif

K_SEM_DEFINE(wakeup_sem, 0, 1);
static struct gpio_callback cb_p0;
static struct gpio_callback cb_p1;
static uint32_t last_activity = 0;

static uint8_t debounce_p, debounce_r, debounce_e, tap_hold_delay, second_tap_delay;
static uint8_t current_layer = 0;
static uint8_t last_layer = 0;
static held_mod_key_t held_mod_keys[TAP_HOLD_SIZE_ARRAY] = {0};
static bool some_held_mod_keys = false;
static struct timeout_tapped_keys timeout_tapped_keys_data;

/* Advertising parameters: connectable, no timeout */
#define BT_LE_ADV_CONN_FOREVER BT_LE_ADV_PARAM( \
    BT_LE_ADV_OPT_CONNECTABLE,                  \
    BT_GAP_ADV_FAST_INT_MIN_2,                  \
    BT_GAP_ADV_FAST_INT_MAX_2,                  \
    NULL)

/* HID modifier bits */
#define MOD_NONE 0x00
#define MOD_LSHIFT HID_KBD_MODIFIER_LEFT_SHIFT

/* Report buffers */
static uint8_t report[9] = {0};
static uint8_t boot_report[9] = {0};
static uint8_t report_consumer[7] = {0};

/* ===== FUNCTIONS ===== */
static inline bool is_modifier(uint16_t keycode)
{
    return keycode >= 0xE0 && keycode <= 0xE7;
}

static inline uint8_t modifier_bit(uint16_t keycode)
{
    return 1 << (((uint8_t)(keycode & 0xFF)) - 0xE0);
}

static void debounce_init(void)
{
    debounce_p = DEBOUNCE_PRESS * CYCLE_BASE_DELAY / CYCLE_DELAY;
    debounce_r = DEBOUNCE_RELEASE * CYCLE_BASE_DELAY / CYCLE_DELAY;
    debounce_e = DEBOUNCE_ENCODER * CYCLE_BASE_DELAY / CYCLE_DELAY;
    tap_hold_delay = TAP_HOLD_DELAY * CYCLE_BASE_DELAY / CYCLE_DELAY;
    second_tap_delay = SECOND_TAP_DELAY * CYCLE_BASE_DELAY / CYCLE_DELAY;
}

static void reports_init(void)
{
    report[0] = BMK_HID_REPORT_ID_KEYBOARD;
    report_consumer[0] = BMK_HID_REPORT_ID_CONSUMER;
}

static void keymap_init(void)
{
    for (uint8_t i = 0; i < LAYERS; i++)
    {
        for (uint8_t j = 0; j < MATRIX_COLS * MATRIX_ROWS; j++)
        {
            keys[j].kc[i] = layers[i][j];
        }
    }
#ifdef ENCODERS
    for (uint8_t l = 0; l < LAYERS; l++)
    {
        for (uint8_t e = 0; e < ENCODERS; e++)
        {
            encoder_keys[e].left_kc[l] = layers[l][MATRIX_COLS * MATRIX_ROWS + e * ENCODER_PINS];
            encoder_keys[e].right_kc[l] = layers[l][MATRIX_COLS * MATRIX_ROWS + e * ENCODER_PINS + 1];
        }
    }
#endif
}

/* ==================== USB HID ==================== */

static const struct device *usb_hid_dev;
static volatile bool usb_configured;

static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
    switch (status)
    {
    case USB_DC_CONFIGURED:
        usb_configured = true;
        LOG_INF("USB configured");
        break;
    case USB_DC_DISCONNECTED:
    case USB_DC_SUSPEND:
        usb_configured = false;
        LOG_INF("USB disconnected/suspended");
        break;
    default:
        break;
    }
}

static void usb_int_in_ready(const struct device *dev)
{
    /* EP ready */
}

static const struct hid_ops usb_ops = {
    .int_in_ready = usb_int_in_ready,
};

static int usb_send_report(const thread_report_t *report)
{
    int err;

    if (!usb_configured)
    {
        return -ENOTCONN;
    }

    if (report->type == BMK_KEYBOARD)
    {
        err = hid_int_ep_write(usb_hid_dev, report->report, 9, NULL);
    }
    else if (report->type == BMK_CONSUMER)
    {
        err = hid_int_ep_write(usb_hid_dev, report->report_consumer, 7, NULL);
    }
    else
    {
        return -EINVAL;
    }

    return err;
}

/* ==================== BLE HID ==================== */

static bool ble_notify_enabled;
static bool boot_notify_enabled;
static struct bt_conn *current_conn;
static uint8_t protocol_mode = 0x01;

static ssize_t read_hid_info(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             void *buf, uint16_t len, uint16_t offset)
{
    static const uint8_t info[] = {0x11, 0x01, 0x00, 0x02};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, info, sizeof(info));
}

static ssize_t read_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, hid_report_map,
                             sizeof(hid_report_map));
}

static ssize_t read_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, report, sizeof(report));
}

static void report_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    ble_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Report notifications %s", ble_notify_enabled ? "enabled" : "disabled");
}

static ssize_t read_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                               void *buf, uint16_t len, uint16_t offset)
{
    static const uint8_t ref[] = {0x00, 0x01};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, ref, sizeof(ref));
}

static ssize_t read_boot_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, boot_report, sizeof(boot_report));
}

static void boot_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    boot_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Boot notifications %s", boot_notify_enabled ? "enabled" : "disabled");
}

static ssize_t read_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                  void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &protocol_mode,
                             sizeof(protocol_mode));
}

static ssize_t write_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    const uint8_t *val = buf;
    if (len != 1 || offset != 0)
    {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    protocol_mode = *val;
    LOG_INF("Protocol mode: %s", protocol_mode ? "Report" : "Boot");
    return len;
}

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    return len;
}

/*
 * HID GATT Service attribute index map:
 * [6]  Report char value (Report Protocol notify)
 * [10] Boot KB Input char value (Boot Protocol notify)
 */
BT_GATT_SERVICE_DEFINE(
    hid_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_hid_info, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
                           BT_GATT_PERM_READ, read_report_map, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_report, NULL, NULL),
    BT_GATT_CCC(report_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
                       read_report_ref, NULL, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_BOOT_KB_IN_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_boot_report, NULL, NULL),
    BT_GATT_CCC(boot_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_PROTOCOL_MODE,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
                           read_protocol_mode, write_protocol_mode, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
                           BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE, NULL, write_ctrl_point, NULL));

static int ble_send_report(thread_report_t *report)
{
    int err;
    uint8_t *buf;
    const struct bt_gatt_attr *attr;

    if (!current_conn)
    {
        return -ENOTCONN;
    }

    if (protocol_mode == 0x00 && boot_notify_enabled)
    {
        buf = boot_report;
        attr = &hid_svc.attrs[10];
    }
    else if (ble_notify_enabled)
    {
        attr = &hid_svc.attrs[6];
    }
    else
    {
        return -ENOTCONN;
    }
    if (report->type == BMK_KEYBOARD)
    {
        buf = report->report;
        err = bt_gatt_notify(current_conn, attr, buf, 9);
        if (err)
            return err;
    }
    else if (report->type == BMK_CONSUMER)
    {
        buf = report->report_consumer;
        err = bt_gatt_notify(current_conn, attr, buf, 7);
        if (err)
            return err;
    }
    return 0;
}

static int send_report(thread_report_t *report)
{
    if (usb_configured)
    {
        return usb_send_report(report);
    }
    return ble_send_report(report);
}

/* ==================== Connection Management ==================== */

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(
        BT_DATA_UUID16_ALL,
        BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
        BT_UUID_16_ENCODE(BT_UUID_DIS_VAL),
        BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0xC1, 0x03),
};

static const struct bt_data sd[] = {
    BT_DATA(
        BT_DATA_NAME_COMPLETE,
        CONFIG_BT_DEVICE_NAME,
        sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(void)
{
    int err = bt_le_adv_start(
        BT_LE_ADV_CONN_FOREVER,
        ad,
        ARRAY_SIZE(ad),
        sd,
        ARRAY_SIZE(sd));
    if (err && err != -EALREADY)
    {
        LOG_ERR("Advertising failed (err %d)", err);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_ERR("Connection failed (err 0x%02x)", err);
        start_advertising();
        return;
    }
    LOG_INF("BLE connected");
    current_conn = bt_conn_ref(conn);
    bt_conn_set_security(conn, BT_SECURITY_L2);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("BLE disconnected (reason 0x%02x)", reason);
    if (current_conn)
    {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    ble_notify_enabled = false;
    boot_notify_enabled = false;
    protocol_mode = 0x01;
    start_advertising();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    if (err)
    {
        LOG_ERR("Security failed (err %d)", err);
    }
    else
    {
        LOG_INF("Security level %d", level);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

static void auth_cancel(struct bt_conn *conn)
{
    LOG_INF("Pairing cancelled");
}

static void auth_pairing_complete(struct bt_conn *conn, bool bonded)
{
    LOG_INF("Pairing complete (bonded=%d)", bonded);
}

static struct bt_conn_auth_cb auth_cb = {
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = auth_pairing_complete,
};

/* ==================== Key functions ==================== */

static void add_held_mod_keys(uint8_t idx)
{
    for (uint8_t i = 0; i < TAP_HOLD_SIZE_ARRAY; i++)
    {
        if (held_mod_keys[i].idx == 0)
        {
            held_mod_keys[i].idx = idx;
            held_mod_keys[i].layer = current_layer;
        }
    }
    some_held_mod_keys = true;
}

static void remove_held_mod_keys(uint8_t idx)
{
    some_held_mod_keys = false;
    for (uint8_t i = 0; i < TAP_HOLD_SIZE_ARRAY; i++)
    {
        if (held_mod_keys[i].idx == idx)
        {
            held_mod_keys[i] = (held_mod_key_t){0};
        }
        else if (held_mod_keys[i].idx != 0)
        {
            some_held_mod_keys = true;
        }
    }
}

static void held_mod_keys_to_report(void)
{
    for (uint8_t i = 0; i < TAP_HOLD_SIZE_ARRAY; i++)
    {
        if (held_mod_keys[i].idx != 0)
        {
            uint16_t keycode = (uint16_t)(keys[held_mod_keys[i].idx].kc[held_mod_keys[i].layer] >> 8);
            report[1] |= modifier_bit(keycode);
            keys[held_mod_keys[i].idx].status = PRESSED;
            held_mod_keys[i] = (held_mod_key_t){0};
        }
    }
    some_held_mod_keys = false;
}

static void tapped_key_release_delayer(struct k_work *work)
{
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct timeout_tapped_keys *ctx = CONTAINER_OF(dwork, struct timeout_tapped_keys, dwork);
    uint8_t idx = ctx->key_idx;
    if (keys[idx].status == TAPPED)
    {
        keys[idx].status = RELEASED;
    }
}

static bool add_report_to_send(bmk_report_type_t type)
{
    thread_report_t th_report = {
        .type = type,
        .report = {0},
        .report_consumer = {0}

    };
    if (type == BMK_KEYBOARD)
    {
        memcpy(th_report.report, report, sizeof(report));
    }
    else if (type == BMK_CONSUMER)
    {
        memcpy(th_report.report_consumer, report_consumer, sizeof(report_consumer));
    }
    int ret = k_msgq_put(&report_msgq, &th_report, K_NO_WAIT);
    last_activity = k_uptime_get_32();
    return (ret == 0);
}

static int release_all()
{
    memset(report, 0, sizeof(report));
    memset(report_consumer, 0, sizeof(report_consumer));
    reports_init();
    add_report_to_send(BMK_KEYBOARD);
    add_report_to_send(BMK_CONSUMER);

    return 1;
}

static int press_key(uint16_t keycode)
{
    uint8_t idx = 0;
    switch (keycode & 0xF000)
    {
    case K_KEYBOARD:
        if (is_modifier(keycode))
        {
            idx = 1;
            report[idx] |= modifier_bit(keycode);
            add_report_to_send(BMK_KEYBOARD);
        }
        else
        {
            // Find free index to send keycode
            for (uint8_t i = 3; i < 9; i++)
            {
                if (report[i] == 0)
                {
                    idx = i;
                    break;
                }
            }
            if (idx != 0)
            {
                // Tap hold keys
                if (some_held_mod_keys)
                {
                    held_mod_keys_to_report();
                }
                // Press key
                report[idx] = (uint8_t)keycode;
                add_report_to_send(BMK_KEYBOARD);
            }
            else
            {
                // No free index
                return -1;
            }
        }
        break;
    case K_CONSUMER:
        // Find free index to send keycode
        for (uint8_t i = 1; i < 7; i++)
        {
            if (report_consumer[i] == 0)
            {
                idx = i;
                break;
            }
        }
        if (idx != 0)
        {
            // Press key
            uint8_t code = (uint8_t)(keycode & 0xFF);
            report_consumer[idx] = code;
            add_report_to_send(BMK_CONSUMER);
        }
        else
        {
            // No free index
            return -1;
        }
        break;
    case K_FN:
        uint8_t layer = keycode & 0xFF;
        switch ((keycode >> 8) & 0x0F)
        {
        case 1: // MO
            if (layer != current_layer && layer < LAYERS)
            {
                last_layer = current_layer;
                current_layer = layer;
                return release_all();
            }
            break;
        }
        break;
    }
    return idx;
}

static int release_key(uint16_t keycode, bool send)
{
    uint8_t idx = 0;
    switch (keycode & 0xF000)
    {
    case K_KEYBOARD:
        if (is_modifier(keycode))
        {
            idx = 1;
            report[idx] &= ~modifier_bit(keycode);
            add_report_to_send(BMK_KEYBOARD);
        }
        else
        {
            // Find pressed keycode
            for (uint8_t i = 3; i < 9; i++)
            {
                if (report[i] == keycode)
                {
                    // Release key
                    report[i] = 0;
                    idx = i;
                    if (send)
                    {
                        add_report_to_send(BMK_KEYBOARD);
                    }
                }
            }
            if (idx == 0)
            {
                // Key is not pressed
                return -1;
            }
        }
        break;
    case K_CONSUMER:
        // Find pressed keycode
        for (uint8_t i = 1; i < 7; i++)
        {
            if (report_consumer[i] == (uint8_t)(keycode & 0xFF))
            {
                // Release key
                report_consumer[i] = 0;
                idx = i;
                if (send)
                {
                    add_report_to_send(BMK_CONSUMER);
                }
            }
        }
        if (idx == 0)
        {
            // Key is not pressed
            return -1;
        }
        break;
    case K_FN:
        uint8_t layer = keycode & 0xFF;
        switch ((keycode >> 8) & 0x0F)
        {
        case 1: // MO
            if (layer == current_layer)
            {
                current_layer = last_layer;
                last_layer = layer;
                return release_all();
            }
            break;
        case 2: // TO
            if (layer < LAYERS)
            {
                current_layer = layer;
            }
            return release_all();
            break;
        }
    }
    return idx;
}

/* ================================================ *\
|* ==================== MATRIX ==================== *|
\* ================================================ */

int matrix_init(void)
{
    int err;

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        if (!gpio_is_ready_dt(&cols[c]))
        {
            LOG_ERR("Col %d GPIO not ready", c);
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&cols[c], GPIO_OUTPUT_INACTIVE);
        if (err)
        {
            LOG_ERR("Col %d config failed (err %d)", c, err);
            return err;
        }
    }

    for (int r = 0; r < MATRIX_ROWS; r++)
    {
        if (!gpio_is_ready_dt(&rows[r]))
        {
            LOG_ERR("Row %d GPIO not ready", r);
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&rows[r], GPIO_INPUT | GPIO_PULL_DOWN);
        if (err)
        {
            LOG_ERR("Row %d config failed (err %d)", r, err);
            return err;
        }
    }

#ifdef ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        if (!gpio_is_ready_dt(&encoders[e]))
        {
            LOG_ERR("Encoder pin %d GPIO not ready", e);
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&encoders[e], GPIO_INPUT | GPIO_PULL_UP);
        if (err)
        {
            LOG_ERR("Encoder pin %d config failed (err %d)", e, err);
            return err;
        }
    }
#endif

#ifdef POWER_EXT
    if (!gpio_is_ready_dt(&power_ext))
    {
        LOG_ERR("External power GPIO not ready");
        return -ENODEV;
    }
    err = gpio_pin_configure_dt(&power_ext, GPIO_OUTPUT_ACTIVE);
    if (err)
    {
        LOG_ERR("External power config failed (err %d)", err);
        return err;
    }
#endif

    LOG_INF("Matrix initialized: %d cols x %d rows", MATRIX_COLS, MATRIX_ROWS);
    return 0;
}

void matrix_scan()
{
    uint8_t idx = 0;
    for (int c = 0; c < MATRIX_COLS; c++)
    {
        /* Drive this column high */
        gpio_pin_set_dt(&cols[c], 1);

        /* Short delay for signal to settle */
        k_busy_wait(5);

        /* Read all rows */
        for (int r = 0; r < MATRIX_ROWS; r++)
        {
            idx = r * MATRIX_COLS + c;
            if (!keys[idx].kc[current_layer])
                continue;
            // Trans keys logic
            uint8_t layer = current_layer;
            while (layer > 0 && keys[idx].kc[layer] == HID_KEY_TRANS)
            {
                layer--;
            }
            // Gey keycode
            uint16_t keycode = keys[idx].kc[layer];
            // If row is high
            if (gpio_pin_get_dt(&rows[r]))
            {
                if (keys[idx].status != PRESSED)
                {
                    if (keys[idx].debounce_count > debounce_p)
                    {
                        // Special keys
                        if ((keycode & 0xF000) == K_SPECIAL)
                        {
                            switch (keycode)
                            {
#ifdef POWER_EXT
                            case HID_KEY_POWER_ON:
                                gpio_pin_set_dt(&power_ext, 1);
                                break;
                            case HID_KEY_POWER_OFF:
                                gpio_pin_set_dt(&power_ext, 0);
                                break;
#endif
                            }
                        }
                        // Tap hold keys
                        if ((keycode & 0xF000) == K_TAP_HOLD)
                        {
                            // Use debounce_count to know when do hold key
                            if (keys[idx].debounce_count > tap_hold_delay)
                            {
                                keycode = (uint16_t)(keycode >> 8);
                                remove_held_mod_keys(idx);
                            }
                            // Second fast tap key
                            else if (keys[idx].status == TAPPED)
                            {
                                if (keys[idx].debounce_count > debounce_p + second_tap_delay)
                                {
                                    keycode = (uint16_t)(keycode & 0xFF);
                                }
                                else
                                {
                                    keys[idx].debounce_count++;
                                    continue;
                                }
                            }
                            else
                            {
                                if (keys[idx].status != HELD)
                                {
                                    keys[idx].status = HELD;
                                    add_held_mod_keys(idx);
                                }
                                keys[idx].debounce_count++;
                                continue;
                            }
                        }
                        // Press key without duplicates
                        release_key(keycode, false);
                        int res = press_key(keycode);
                        if (res != -1)
                        {
                            keys[idx].status = PRESSED;
                            keys[idx].debounce_count = 0;
                        }
                    }
                    else
                    {
                        keys[idx].debounce_count++;
                    }
                }
                else
                {
                    if (keys[idx].debounce_count != 0)
                    {
                        keys[idx].debounce_count--;
                    }
                }
            }
            else
            {
                if (keys[idx].status == PRESSED)
                {
                    if (keys[idx].debounce_count > debounce_r)
                    {
                        // Tap hold keys
                        if ((keycode & 0xF000) == K_TAP_HOLD)
                        {
                            // TODO improvement this knowing what key is pressed, mod or tapped key.
                            release_key((uint16_t)(keycode & 0xFF), false);
                            keycode = (uint16_t)(keycode >> 8);
                        }
                        int res = release_key(keycode, true);
                        if (res != -1)
                        {
                            keys[idx].status = RELEASED;
                            keys[idx].debounce_count = 0;
                        }
                    }
                    else
                    {
                        keys[idx].debounce_count++;
                    }
                }
                else if (keys[idx].status == HELD) // Tap hold keys
                {
                    if (keys[idx].debounce_count > debounce_r)
                    {
                        keys[idx].debounce_count = 0;
                        continue;
                    }
                    else if (keys[idx].debounce_count == debounce_r)
                    {
                        remove_held_mod_keys(idx);
                        // Do tap key (press and release)
                        keycode = (uint16_t)(keycode & 0xFF);
                        int res = press_key(keycode);
                        if (res != -1)
                        {
                            release_key(keycode, true);
                            keys[idx].status = TAPPED;
                            keys[idx].debounce_count = 0;
                            timeout_tapped_keys_data.key_idx = idx;
                            k_work_reschedule(&timeout_tapped_keys_data.dwork, K_MSEC(TAP_HOLD_DELAY));
                        }
                    }
                    else
                    {
                        keys[idx].debounce_count++;
                    }
                }
                else
                {
                    if (keys[idx].debounce_count != 0)
                    {
                        keys[idx].debounce_count--;
                    }
                }
            }
        }

        /* Drive column low again */
        gpio_pin_set_dt(&cols[c], 0);
    }

#ifdef ENCODERS
    for (uint8_t e = 0; e < ENCODERS; e += 2)
    {
        uint8_t left = gpio_pin_get_dt(&encoders[e * ENCODER_PINS]);
        uint8_t right = gpio_pin_get_dt(&encoders[e * ENCODER_PINS + 1]);
        uint8_t current_value = left << 1;
        current_value |= right;
        uint8_t last_value = encoder_keys[e].last_value;
        // If encoder is at rest position
        if (!current_value && last_value)
        {
            if (encoder_keys[e].debounce_count > debounce_e)
            {
                // Get keycode based on direction, negative left, positive right
                uint16_t keycode = 0;
                if (encoder_keys[e].direction < 0)
                {
                    uint8_t layer = current_layer;
                    while (layer > 0 && encoder_keys[e].left_kc[layer] == HID_KEY_TRANS)
                    {
                        layer--;
                    }
                    keycode = encoder_keys[e].left_kc[layer];
                }
                else
                {
                    uint8_t layer = current_layer;
                    while (layer > 0 && encoder_keys[e].right_kc[layer] == HID_KEY_TRANS)
                    {
                        layer--;
                    }
                    keycode = encoder_keys[e].right_kc[layer];
                }
                // Reset values
                encoder_keys[e].last_value = encoder_keys[e].direction = encoder_keys[e].debounce_count = 0;
                // Send keycode
                press_key(keycode);
                release_key(keycode, true);
            }
            else
            {
                encoder_keys[e].debounce_count++;
            }
        }
        else
        {

            if (current_value != last_value)
            {
                switch ((last_value << 2) | current_value)
                {
                // left
                case 0b0010:
                case 0b1011:
                case 0b1101:
                case 0b0100:
                    encoder_keys[e].direction--;
                    break;
                // right
                case 0b0001:
                case 0b0111:
                case 0b1110:
                case 0b1000:
                    encoder_keys[e].direction++;
                    break;
                default:
                    break;
                }
            }
            encoder_keys[e].last_value = current_value;
        }
    }
#endif
}

/* =============================================== *\
|* ==================== SLEEP ==================== *|
\* =============================================== */

void universal_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    k_sem_give(&wakeup_sem);
}

void sleep_init(void)
{
    // Obtenemos los dispositivos de los dos puertos
    const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    const struct device *gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    // Configuramos el callback para el Puerto 0 (P0.00 a P0.31)
    gpio_init_callback(&cb_p0, universal_handler, 0xFFFFFFFF);
    gpio_add_callback(gpio0_dev, &cb_p0);

    // Configuramos el callback para el Puerto 1 (P1.00 a P1.15)
    // El puerto 1 solo tiene 16 pines, pero 0xFFFFFFFF no le hace daño
    gpio_init_callback(&cb_p1, universal_handler, 0xFFFFFFFF);
    gpio_add_callback(gpio1_dev, &cb_p1);
}

void matrix_sleep(void)
{
    for (int r = 0; r < MATRIX_ROWS; r++)
    {
        gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_EDGE_RISING);
    }

#ifdef ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        if (gpio_pin_get_dt(&encoders[e]))
        {
            gpio_pin_interrupt_configure_dt(&encoders[e], GPIO_INT_EDGE_RISING);
        }
        else
        {
            gpio_pin_interrupt_configure_dt(&encoders[e], GPIO_INT_EDGE_FALLING);
        }
    }
#endif

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        gpio_pin_set_dt(&cols[c], 1);
    }
}

void matrix_wakeup(void)
{
    for (int r = 0; r < MATRIX_ROWS; r++)
    {
        gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_DISABLE);
        gpio_pin_configure_dt(&rows[r], GPIO_INPUT | GPIO_PULL_DOWN);
    }

#ifdef ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        gpio_pin_interrupt_configure_dt(&encoders[e], GPIO_INT_DISABLE);
        gpio_pin_configure_dt(&encoders[e], GPIO_INPUT | GPIO_PULL_UP);
    }
#endif

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        gpio_pin_set_dt(&cols[c], 0);
    }
}

/* ================================================ *\
|* ===================== SEND ===================== *|
\* ================================================ */

void sender_thread(void *p1, void *p2, void *p3)
{
    thread_report_t report_temp;
    int err;

    while (1)
    {
        k_msgq_get(&report_msgq, &report_temp, K_FOREVER);

        bool sent = false;
        uint8_t retries = 0;

        while (!sent && retries < 200)
        {
            err = send_report(&report_temp);

            if (err == 0)
            {
                sent = true;
            }
            else
            {
                retries++;
                k_sleep(K_MSEC(5));
            }
        }
    }
}

/* ================================================ *\
|* ===================== MAIN ===================== *|
\* ================================================ */

k_tid_t threads_init()
{
    k_tid_t tid = k_thread_create(
        &send_thread_data, send_thread_area,
        K_THREAD_STACK_SIZEOF(send_thread_area),
        sender_thread,
        NULL, NULL, NULL,
        K_PRIO_COOP(SEND_THREAD_PRIORITY), 0, K_NO_WAIT);
    return tid;
}

void delayed_init(void)
{
    k_work_init_delayable(&timeout_tapped_keys_data.dwork, tapped_key_release_delayer);
}

int main(void)
{
    int err;

    LOG_INF("BMK Keyboard starting...");

    /* BLE init first -- always available */
    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return 0;
    }
    LOG_INF("Bluetooth ready");

    bt_conn_auth_cb_register(&auth_cb);
    bt_conn_auth_info_cb_register(&auth_info_cb);

    bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
    LOG_INF("Bonds cleared");

    start_advertising();
    LOG_INF("Advertising as '%s'", CONFIG_BT_DEVICE_NAME);

    /* USB HID init -- only if VBUS detected */
    if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER))
    {
        LOG_INF("VBUS detected, initializing USB");
        usb_hid_dev = device_get_binding("HID_0");
        if (usb_hid_dev)
        {
            usb_hid_register_device(usb_hid_dev, hid_report_map,
                                    sizeof(hid_report_map), &usb_ops);
            usb_hid_init(usb_hid_dev);
            err = usb_enable(usb_status_cb);
            if (err)
            {
                LOG_WRN("USB enable failed (err %d)", err);
            }
            else
            {
                LOG_INF("USB HID ready");
            }
        }
    }
    else
    {
        LOG_INF("No VBUS -- battery mode, BLE only");
    }

    debounce_init();
    reports_init();
    keymap_init();
    sleep_init();
    matrix_init();
    threads_init();
    delayed_init();

    while (1)
    {
        if (k_uptime_get_32() - last_activity > SLEEP_TIMEOUT)
        {
            matrix_sleep();
            while (k_sem_take(&wakeup_sem, K_NO_WAIT) == 0)
                ;
            k_sem_take(&wakeup_sem, K_FOREVER);
            matrix_wakeup();
            last_activity = k_uptime_get_32();
        }
        matrix_scan();
        k_usleep(CYCLE_DELAY);
    }

    return 0;
}
