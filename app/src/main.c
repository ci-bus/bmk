/*
 * BMK Keyboard Firmware
 * USB HID + BLE HID with Boot Protocol
 * Priority: USB when connected, BLE otherwise
 */

#include "main.h"

#if LOGS
LOG_MODULE_REGISTER(bmk, LOG_LEVEL);
#endif

static struct gpio_dt_spec cols[MATRIX_COLS] = {0};
static struct gpio_dt_spec rows[MATRIX_ROWS] = {0};
#if ENCODERS
static struct gpio_dt_spec encoders[ENCODER_PINS] = {0};
#endif
#if POWER_EXT
static struct gpio_dt_spec power_ext = {0};
#endif

#if RGB
#define STRIP_NODE DT_NODELABEL(ws2812)
#define NUM_LEDS DT_PROP(STRIP_NODE, chain_length)
const struct device *spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi3));
static const struct device *strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb pixels[NUM_LEDS];
static uint8_t rgb_color = 0;
static uint8_t rgb_light = 255;
static uint8_t rgb_saturation = 255;
static bool rgb_on = RGB_ON_STARTUP;
static bool rgb_spi_suspended = false;
#if RGB_EFFECTS
static uint8_t rgb_beat = 0;
#endif
#endif

#if POWER_EXT
static bool power_ext_on = POWER_EXT_ON;
#endif

#if RGB || POWER_EXT
static struct k_work_delayable rgb_power_ext_work;
#endif

K_THREAD_STACK_DEFINE(send_thread_area, SEND_THREAD_STACK_SIZE);
struct k_thread send_thread_data;

K_MSGQ_DEFINE(report_msgq, sizeof(thread_report_t), SEND_THREAD_CACHE_SIZE, 4);

static uint8_t abs_cols_pins[MATRIX_COLS] = {0};
static uint8_t abs_rows_pins[MATRIX_ROWS] = {0};

static struct key keys[MATRIX_COLS * MATRIX_ROWS] = {0};

#if ENCODERS
static struct encoder_key encoder_keys[ENCODERS] = {0};
#endif

#if BATTERY
static uint8_t battery_percent = 100;
#endif

#if USB
static bool usb_connected = true;
static int32_t last_check_usb = 0;
#endif

K_SEM_DEFINE(sleep_sem, 0, 1);
static struct gpio_callback cb_p0;
static struct gpio_callback cb_p1;
static int32_t last_activity = 0;

static uint8_t debounce_p, debounce_r, debounce_e, tap_hold_p, tap_hold_r;
static uint8_t current_layer = 0;
static uint8_t last_layer = 0;
static held_mod_key_t held_mod_keys[TAP_HOLD_SIZE_ARRAY] = {0};
static bool some_held_mod_keys = false;
static bool awake = false;
static bool deep_sleep = false;
static bool ble_resetting = false;
static bool ble_advertising = false;

static bool ble_notify_enabled;
static bool boot_notify_enabled;
static struct bt_conn *current_conn = NULL;
static uint8_t protocol_mode = 0x01;

/* Advertising parameters: connectable, no timeout */
#define BT_LE_ADV_CONN_FOREVER BT_LE_ADV_PARAM( \
    BT_LE_ADV_OPT_CONNECTABLE,                  \
    BT_GAP_ADV_FAST_INT_MIN_2,                  \
    BT_GAP_ADV_FAST_INT_MAX_2,                  \
    NULL)

static struct bt_le_adv_param *param = BT_LE_ADV_CONN_FOREVER;

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
    tap_hold_p = TAP_HOLD_PRESS_DELAY * CYCLE_BASE_DELAY / CYCLE_DELAY;
    tap_hold_r = TAP_HOLD_RELEASE_DELAY * CYCLE_BASE_DELAY / CYCLE_DELAY;
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
#if ENCODERS
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

/* ================================================ *\
|* ===================== RGB ====================== *|
\* ================================================ */

#if RGB
struct led_rgb hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v)
{
    struct led_rgb rgb;
    uint8_t region, remainder, p, q, t;

    if (s == 0)
    {
        rgb.r = rgb.g = rgb.b = v;
        return rgb;
    }

    region = h / 43;
    remainder = (h % 43) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
    case 0:
        rgb.r = v;
        rgb.g = t;
        rgb.b = p;
        break;
    case 1:
        rgb.r = q;
        rgb.g = v;
        rgb.b = p;
        break;
    case 2:
        rgb.r = p;
        rgb.g = v;
        rgb.b = t;
        break;
    case 3:
        rgb.r = p;
        rgb.g = q;
        rgb.b = v;
        break;
    case 4:
        rgb.r = t;
        rgb.g = p;
        rgb.b = v;
        break;
    default:
        rgb.r = v;
        rgb.g = p;
        rgb.b = q;
        break;
    }
    return rgb;
}

static void hsv_to_leds(uint8_t h, uint8_t s, uint8_t v)
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        pixels[i] = hsv_to_rgb(h, s, v);
    }
    led_strip_update_rgb(strip, pixels, NUM_LEDS);
}

void rgb_leds_update(void)
{
    if (!device_is_ready(strip))
    {
        return;
    }

    hsv_to_leds(rgb_color, rgb_saturation, rgb_light);
}
#if RGB_EFFECTS
#if RGB_EFF_RAINBOW
void rgb_eff_rainbow()
{
    uint8_t sum = 255 / NUM_LEDS;
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        uint8_t h = (rgb_beat + (i * sum));
        pixels[i] = hsv_to_rgb(h, rgb_saturation, rgb_light);
    }
}
#endif
#if RGB_EFF_COLORS
void rgb_eff_colors()
{
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        pixels[i] = hsv_to_rgb(rgb_beat, rgb_saturation, rgb_light);
    }
}
#endif
#if RGB_EFF_KITT
void rgb_eff_kitt(void)
{
    // Calcule 2x leds num to right or left direction
    uint8_t led_main = rgb_beat % (NUM_LEDS * 2);
    if (led_main < NUM_LEDS) // Right direction
    {
        uint8_t v = rgb_light;
        for (int i = led_main; i >= 0; i--)
        {
            pixels[i] = hsv_to_rgb(rgb_color, rgb_saturation, v);
            v = v / 4;
        }
        v = rgb_light / 4;
        for (uint8_t i = led_main + 1; i < NUM_LEDS; i++)
        {
            pixels[i] = hsv_to_rgb(rgb_color, rgb_saturation, v);
            v = v / 4;
        }
    }
    else // Left direction
    {
        led_main = (rgb_beat % (NUM_LEDS * 2)) - NUM_LEDS;
        uint8_t v = rgb_light;
        for (int i = led_main; i >= 0; i--)
        {
            pixels[NUM_LEDS - i - 1] = hsv_to_rgb(rgb_color, rgb_saturation, v);
            v = v / 4;
        }
        v = rgb_light / 4;
        for (uint8_t i = led_main + 1; i < NUM_LEDS; i++)
        {
            pixels[NUM_LEDS - i - 1] = hsv_to_rgb(rgb_color, rgb_saturation, v);
            v = v / 4;
        }
    }
    if (rgb_beat == (NUM_LEDS * 2))
    {
        rgb_beat = 0;
    }
}
#endif
void rgb_effects_update(void)
{
    rgb_beat++;
    rgb_eff_colors();
    led_strip_update_rgb(strip, pixels, NUM_LEDS);
}
static void rgb_work_handler(struct k_work *work)
{
    rgb_effects_update();
}
K_WORK_DEFINE(rgb_work_obj, rgb_work_handler);

static void rgb_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&rgb_work_obj);
}
K_TIMER_DEFINE(rgb_periodic_timer, rgb_timer_handler, NULL);
void rgb_start_periodic_task(void)
{
    k_timer_start(&rgb_periodic_timer, K_NO_WAIT, K_MSEC(60));
}
void rgb_stop_periodic_task(void)
{
    k_timer_stop(&rgb_periodic_timer);
    rgb_beat = 0;
}
#endif
#endif

/* ============= RGB + POWER_EXT ============= */
#if RGB || POWER_EXT
static void rgb_power_ext_delayer(struct k_work *work)
{

#if POWER_EXT && POWER_EXT_RGB_LINKED
    gpio_pin_set_dt(&power_ext, rgb_on);
    k_sleep(K_MSEC(POWER_EXT_RGB_DELAY));
#elif POWER_EXT
    gpio_pin_set_dt(&power_ext, power_ext_on);
#endif

    if (rgb_on)
    {
        rgb_leds_update();
    }
}
static void rgb_power_ext_update(void)
{
    k_work_reschedule(&rgb_power_ext_work, K_MSEC(POWER_EXT_RGB_DELAY));
}

static void rgb_spi_off(void)
{
    if (!rgb_spi_suspended)
    {
        pm_device_action_run(spi_dev, PM_DEVICE_ACTION_SUSPEND);
        rgb_spi_suspended = true;
    }
}
static void rgb_spi_on(void)
{
    if (rgb_spi_suspended)
    {
        pm_device_action_run(spi_dev, PM_DEVICE_ACTION_RESUME);
        rgb_spi_suspended = false;
    }
}
#endif

/* ================================================ *\
|* ===================== USB ====================== *|
\* ================================================ */

#if USB

static const struct device *usb_hid_dev;

void bmk_check_usb(void)
{
    if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER))
    {
        if (!usb_connected)
        {
            usb_enable(NULL);
            usb_connected = true;
            last_activity = k_uptime_get_32();
        }
    }
    else
    {
        if (usb_connected)
        {
            usb_disable();
            usb_connected = false;
            last_activity = k_uptime_get_32();
        }
    }
}

static void usb_int_in_ready(const struct device *dev) {}

static const struct hid_ops usb_ops = {
    .int_in_ready = usb_int_in_ready,
};

static int usb_send_report(const thread_report_t *report)
{
    int err;

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

#endif

/* ==================== BLE HID ==================== */

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
#if USB
    if (usb_connected)
    {
        return usb_send_report(report);
    }
#endif
    return ble_send_report(report);
}

/* ============= BATTERY LEVEL ============= */
#if BATTERY
void battery_update(void)
{
    uint8_t last_battery_percent = battery_percent;
    battery_percent = get_battery();
    if (last_battery_percent != battery_percent)
    {
        bt_bas_set_battery_level(battery_percent);
    }
}
static void bat_work_handler(struct k_work *work)
{
    battery_update();
}
K_WORK_DEFINE(bat_work_obj, bat_work_handler);

static void bat_timer_handler(struct k_timer *dummy)
{
    k_work_submit(&bat_work_obj);
}
K_TIMER_DEFINE(bat_periodic_timer, bat_timer_handler, NULL);
void bat_start_periodic_task(void)
{
#if LOGS
    LOG_INF("Level battery task starting...");
#endif
    k_timer_start(&bat_periodic_timer, K_NO_WAIT, K_SECONDS(BAT_UPDATE));
}
void bat_stop_periodic_task(void)
{
    k_timer_stop(&bat_periodic_timer);
}
#endif

/* =============================================== *\
|* ================= ADVERTISING ================= *|
\* =============================================== */

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
    bt_le_adv_stop();
    int err = bt_le_adv_start(
        param,
        ad,
        ARRAY_SIZE(ad),
        sd,
        ARRAY_SIZE(sd));
    if (err && err != -EALREADY)
    {
#if LOGS
        LOG_ERR("Advertising error: %d", err);
#endif
        k_msleep(200);
        // sys_reboot(SYS_REBOOT_COLD);
    }
    else
    {
        ble_advertising = true;
#if LOGS
        LOG_INF("Advertising...");
#endif
    }
}

/* =============================================== *\
|* ==================== SLEEP ==================== *|
\* =============================================== */

void universal_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    k_sem_give(&sleep_sem);
}

void sleep_init(void)
{
    // Get pin ports
    const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    const struct device *gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    // Config callback for all pins port 0
    gpio_init_callback(&cb_p0, universal_handler, 0xFFFFFFFF);
    gpio_add_callback(gpio0_dev, &cb_p0);

    // Config callback for all pins port 1
    gpio_init_callback(&cb_p1, universal_handler, 0xFFFFFFFF);
    gpio_add_callback(gpio1_dev, &cb_p1);
}

void keyboard_sleep(void)
{
#if LOGS
    LOG_INF("Sleeping...");
#endif
#if BATTERY
    bat_stop_periodic_task();
#endif
#if RGB_EFFECTS
    rgb_stop_periodic_task();
#endif
#if POWER_EXT
    gpio_pin_set_dt(&power_ext, 0);
#endif
#if USB
    bmk_check_usb();
#endif

    for (int r = (MATRIX_ROWS - 1); r >= 0; r--)
    {
        if (r >= MATRIX_ROWS - 8)
        {
            gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_EDGE_TO_ACTIVE);
        }
        else
        {
            gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_LEVEL_HIGH);
        }
    }

#if ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        gpio_pin_interrupt_configure_dt(&encoders[e], GPIO_INT_LEVEL_LOW);
    }
#endif

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        nrf_gpio_pin_set(abs_cols_pins[c]);
    }
    awake = false;
}

void keyboard_deep_sleep(void)
{
#if LOGS
    LOG_INF("Deep sleeping...");
#endif
    bt_disable();
#if USB
    if (usb_connected)
    {
        usb_disable();
        usb_connected = false;
    }
#endif
    last_activity = -(SLEEP_TIMEOUT_MS);
    deep_sleep = true;
}

/* ================================================= *\
|* ============= Connection Management ============= *|
\* ================================================= */

void init_mac_identity(bool retry)
{
    bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
    size_t count = ARRAY_SIZE(addrs);

    bt_id_get(addrs, &count);

    if (count == 0)
    {
#if LOGS
        LOG_WRN("ERROR BT not inited!");
#endif
        return;
    }
    param->id = (count - 1);
    char addr_clean_str[BT_ADDR_STR_LEN];
    bt_addr_to_str(&addrs[param->id].a, addr_clean_str, sizeof(addr_clean_str));
#if LOGS
    LOG_INF("[BLE] MAC identity: %s", addr_clean_str);
#endif
    if (!retry && count < CONFIG_BT_ID_MAX)
    {
        int bt_id = bt_id_create(NULL, NULL);

        if (bt_id > 0)
        {
            param->id = bt_id;
        }
        else
        {
            param->id = BT_ID_DEFAULT;
#if LOGS
            LOG_ERR("ERROR creating MAC identity");
#endif
        }

        init_mac_identity(true);
    }
}

void force_bluetooth_reset(void)
{
    if (!ble_resetting)
    {
        ble_resetting = true;
#if LOGS
        LOG_INF("BLE resetting...");
#endif
        int err = 0;
        if (current_conn != NULL)
        {
            err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            if (err)
            {
#if LOGS
                LOG_ERR("Reset disconnect error: %d", err);
#endif
            }
        }
        err = bt_unpair(param->id, NULL);
        if (err)
        {
#if LOGS
            LOG_ERR("Reset unpair error: %d", err);
#endif
        }
#if LOGS
        LOG_INF("BT ID deleting...");
#endif
        int rc = settings_delete("bt/id");

        if (rc)
        {
#if LOGS
            LOG_ERR("Error deleting ID from flash: %d", rc);
#endif
        }

        k_msleep(200);
        sys_reboot(SYS_REBOOT_COLD);
    }
}

static void connected(struct bt_conn *conn, uint8_t err)
{
#if LOGS
    LOG_INF("Connected...");
#endif
    bt_le_adv_stop();
    ble_advertising = false;
    const bt_addr_le_t *dst_addr = bt_conn_get_dst(conn);
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(dst_addr, addr_str, sizeof(addr_str));

#if LOGS
    LOG_INF("Connected device MAC: %s", addr_str);

#endif

    if (err)
    {
#if LOGS
        LOG_ERR("Connected error: %d", err);
#endif
        int err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (err)
        {
#if LOGS
            LOG_ERR("Connected disconnect error: %d", err);
#endif
            start_advertising();
        }
        return;
    }
    current_conn = bt_conn_ref(conn);
    k_msleep(50);
    bt_security_t current_security = bt_conn_get_security(current_conn);
    if (current_security)
    {
        if (current_security < BT_SECURITY_L2)
        {
#if LOGS
            LOG_INF("Setting L2 security...");
#endif
            int err = bt_conn_set_security(current_conn, BT_SECURITY_L2);
            if (err)
            {
#if LOGS
                LOG_ERR("Security work error: %d", err);
#endif
                int err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                if (err)
                {
#if LOGS
                    LOG_ERR("Security work disconnect error: %d", err);
#endif
                    start_advertising();
                }
            }
        }
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (reason)
    {
#if LOGS
        LOG_INF("Disconnected reason %d", reason);
#endif
    }
    bt_conn_unref(conn);
    current_conn = NULL;
    ble_notify_enabled = false;
    boot_notify_enabled = false;
    protocol_mode = 0x01;
    if (!deep_sleep)
    {
        keyboard_deep_sleep();
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
#if LOGS
    LOG_INF("Security changed");
#endif
    if (err)
    {
#if LOGS
        LOG_ERR("Change security error: %d", err);
#endif
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    else
    {
        if (level >= BT_SECURITY_L2)
        {
#if LOGS
            LOG_INF("Security changed L2 OK!");
#endif
            if (current_conn == NULL)
            {
                current_conn = bt_conn_ref(conn);
            }
        }
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

static void auth_cancel(struct bt_conn *conn)
{
#if LOGS
    LOG_ERR("Auth cancel");
#endif
    // bt_conn_disconnect(conn, BT_HCI_ERR_OP_CANCELLED_BY_HOST);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
#if LOGS
    LOG_INF("Pairing complete!");
#endif
#if BATTERY
    k_msleep(50);
    bat_start_periodic_task();
#endif
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err err)
{
#if LOGS
    LOG_ERR("Pairing error: %d", err);
#endif
    if (err == BT_SECURITY_ERR_PAIR_NOT_SUPPORTED)
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_PAIRING_NOT_SUPPORTED);
    }
    else if (err == BT_SECURITY_ERR_PAIR_NOT_ALLOWED)
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_PAIRING_NOT_ALLOWED);
    }
    else
    {
        bt_conn_disconnect(conn, BT_HCI_ERR_HOST_BUSY_PAIRING);
    }
}

static struct bt_conn_auth_cb auth_cb = {
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

/* ======================================================= *\
|* ======================= WAKE UP ======================= *|
\* ======================================================= */

int keyboard_deep_sleep_wakeup(void)
{
#if LOGS
    LOG_INF("Deep waking up...");
#endif
    int err = bt_enable(NULL);
    if (err)
    {
        return err;
    }

    bt_conn_auth_cb_register(&auth_cb);
    bt_conn_auth_info_cb_register(&auth_info_cb);

#if IS_ENABLED(CONFIG_SETTINGS)
    settings_load();
    k_msleep(100);
#endif

    start_advertising();
    deep_sleep = false;
    return 0;
}

void keyboard_wakeup(void)
{
#if LOGS
    LOG_INF("Waking up...");
#endif
    for (int c = 0; c < MATRIX_COLS; c++)
    {
        gpio_pin_set_dt(&cols[c], 0);
    }

    for (int r = 0; r < MATRIX_ROWS; r++)
    {
        gpio_pin_interrupt_configure_dt(&rows[r], GPIO_INT_DISABLE);
        gpio_pin_configure_dt(&rows[r], GPIO_INPUT | GPIO_PULL_DOWN);
    }

#if ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        gpio_pin_interrupt_configure_dt(&encoders[e], GPIO_INT_DISABLE);
        gpio_pin_configure_dt(&encoders[e], GPIO_INPUT | GPIO_PULL_UP);
    }
#endif

#if RGB || POWER_EXT
    rgb_power_ext_update();
#endif
#if RGB_EFFECTS
    rgb_start_periodic_task();
#endif
#if BATTERY
    bat_start_periodic_task();
#endif
    if (deep_sleep)
    {
        keyboard_deep_sleep_wakeup();
    }
#if USB
    bmk_check_usb();
#endif
    awake = true;
}

/* ======================================================= *\
|* ==================== Key functions ==================== *|
\* ======================================================= */

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
        uint8_t idx = held_mod_keys[i].idx;
        uint8_t layer = held_mod_keys[i].layer;
        if (idx != 0)
        {
            uint16_t keycode = (uint16_t)(keys[idx].kc[layer] >> 8);
            report[1] |= modifier_bit(keycode);
            held_mod_keys[i] = (held_mod_key_t){0};
            keys[idx].held = true;
        }
    }
    some_held_mod_keys = false;
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

static int press_key(uint16_t keycode, bool tap_hold_key)
{
    last_activity = k_uptime_get_32();
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
            // Temp to debug
            /*
            switch (keycode)
            {
            case HID_KEY_ESC:
                hsv_to_leds(0, 0, 0);
                break;
            case HID_KEY_1:
                hsv_to_leds(32, 255, 255);
                break;
            case HID_KEY_2:
                hsv_to_leds(64, 255, 255);
                break;
            case HID_KEY_3:
                hsv_to_leds(96, 255, 255);
                break;
            case HID_KEY_4:
                hsv_to_leds(128, 255, 255);
                break;
            case HID_KEY_5:
                hsv_to_leds(160, 255, 255);
                break;
            case HID_KEY_6:
                hsv_to_leds(192, 255, 255);
                break;
            case HID_KEY_7:
                hsv_to_leds(224, 255, 255);
                break;
            case HID_KEY_8:
                hsv_to_leds(255, 255, 255);
                break;
            }
            */
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
                if (!tap_hold_key && some_held_mod_keys)
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
    case K_SPECIAL:
        switch (keycode)
        {
#if POWER_EXT
        case HID_KEY_POWER_ON:
            power_ext_on = true;
#if POWER_EXT_RGB_LINKED
            rgb_spi_on();
            rgb_on = power_ext_on;
#endif
            rgb_power_ext_update();
            break;
        case HID_KEY_POWER_OFF:
            power_ext_on = false;
#if POWER_EXT_RGB_LINKED
            rgb_on = power_ext_on;
            rgb_spi_off();
#endif
            rgb_power_ext_update();
            break;
#endif
#if RGB
        case HID_KEY_RGB_LIGHT:
            rgb_light = rgb_light + RGB_LIGHT_JUMP;
            rgb_leds_update();
            break;
        case HID_KEY_RGB_COLOR:
            rgb_color = rgb_color + RGB_COLOR_JUMP;
            rgb_leds_update();
            break;
        case HID_KEY_RGB_SATURATION:
            rgb_color = rgb_saturation + HID_KEY_RGB_SATURATION;
            rgb_leds_update();
            break;
        case HID_KEY_RGB_ON:
            rgb_spi_on();
            rgb_on = true;
#if POWER_EXT && POWER_EXT_RGB_LINKED
            power_ext_on = rgb_on;
#endif
            rgb_power_ext_update();
            break;
        case HID_KEY_RGB_OFF:
            rgb_on = false;
            rgb_spi_off();
#if POWER_EXT && POWER_EXT_RGB_LINKED
            power_ext_on = rgb_on;
#endif
            rgb_power_ext_update();
            break;
        case HID_KEY_RGB_TOGGLE:
            if (rgb_on)
            {
                rgb_on = false;
                rgb_spi_off();
            }
            else
            {
                rgb_spi_on();
                rgb_on = true;
            }
#if POWER_EXT && POWER_EXT_RGB_LINKED
            power_ext_on = rgb_on;
#endif
            rgb_power_ext_update();
            break;
#endif
        case HID_BT_CLEAR:
            force_bluetooth_reset();
            break;
        case HID_RESET:
            sys_reboot(SYS_REBOOT_COLD);
            break;
        case HID_BOOT:
            nrf_power_gpregret_set(NRF_POWER, 0, 0x57);
            sys_reboot(SYS_REBOOT_COLD);
            break;
        }
        break;
    }
    return idx;
}

static int release_key(uint16_t keycode, bool send)
{
    last_activity = k_uptime_get_32();
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
    case K_SPECIAL:
        switch (keycode)
        {
        case HID_DEEP_SLEEP:
            release_all();
            keyboard_deep_sleep();
            return 0;
        }
    }
    last_activity = k_uptime_get_32();
    return idx;
}

bool some_pressed_key(void)
{
    return (bool)(report[1] || report[3] || report[4] || report[5] || report[6] || report[7] || report[8]);
}

/* ================================================ *\
|* ==================== MATRIX ==================== *|
\* ================================================ */

void magic_init()
{
    for (int c = 0; c < MATRIX_COLS; c++)
    {
        cols[c] = (struct gpio_dt_spec){
            .port = ((pin_cols[c] >> 8) ? GPIO1 : GPIO0),
            .pin = (gpio_pin_t)(pin_cols[c] & 0xFF),
            .dt_flags = GPIO_ACTIVE_HIGH,
        };
    }
    for (int r = 0; r < MATRIX_ROWS; r++)
    {
        rows[r] = (struct gpio_dt_spec){
            .port = ((pin_rows[r] >> 8) ? GPIO1 : GPIO0),
            .pin = (gpio_pin_t)(pin_rows[r] & 0xFF),
            .dt_flags = GPIO_ACTIVE_HIGH,
        };
    }
#if ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        encoders[e] = (struct gpio_dt_spec){
            .port = ((pin_encoders[e] >> 8) ? GPIO1 : GPIO0),
            .pin = (gpio_pin_t)(pin_encoders[e] & 0xFF),
            .dt_flags = GPIO_ACTIVE_LOW,
        };
    }
#endif
#if POWER_EXT
    power_ext = (struct gpio_dt_spec){
        .port = ((pin_power_ext >> 8) ? GPIO1 : GPIO0),
        .pin = (gpio_pin_t)(pin_power_ext & 0xFF),
        .dt_flags = GPIO_ACTIVE_LOW,
    };
#endif
}

int pins_init(void)
{
    int err;

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        if (!gpio_is_ready_dt(&cols[c]))
        {
#if LOGS
            LOG_ERR("Col %d GPIO not ready", c);
#endif
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&cols[c], GPIO_OUTPUT_INACTIVE);
        if (err)
        {
#if LOGS
            LOG_ERR("Col %d config failed (err %d)", c, err);
#endif
            return err;
        }
        abs_cols_pins[c] = NRF_GPIO_PIN_MAP((cols[c].port == GPIO1), cols[c].pin);
    }

    for (int r = 0; r < MATRIX_ROWS; r++)
    {
        rows[r].pin = pin_rows[r] & 0xFF;
        rows[r].port = (pin_rows[r] >> 8) ? GPIO1 : GPIO0;
        rows[r].dt_flags = GPIO_ACTIVE_HIGH;
        if (!gpio_is_ready_dt(&rows[r]))
        {
#if LOGS
            LOG_ERR("Row %d GPIO not ready", r);
#endif
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&rows[r], GPIO_INPUT | GPIO_PULL_DOWN);
        if (err)
        {
#if LOGS
            LOG_ERR("Row %d config failed (err %d)", r, err);
#endif
            return err;
        }
        abs_rows_pins[r] = NRF_GPIO_PIN_MAP((rows[r].port == GPIO1), rows[r].pin);
    }

#if ENCODERS
    for (int e = 0; e < ENCODERS * ENCODER_PINS; e++)
    {
        if (!gpio_is_ready_dt(&encoders[e]))
        {
#if LOGS
            LOG_ERR("Encoder pin %d GPIO not ready", e);
#endif
            return -ENODEV;
        }
        err = gpio_pin_configure_dt(&encoders[e], GPIO_INPUT | GPIO_PULL_UP);
        if (err)
        {
#if LOGS
            LOG_ERR("Encoder pin %d config failed (err %d)", e, err);
#endif
            return err;
        }
    }
#endif

#if POWER_EXT
    power_ext.pin = pin_power_ext & 0xFF;
    power_ext.port = (pin_power_ext >> 8) ? GPIO1 : GPIO0;
    power_ext.dt_flags = GPIO_ACTIVE_HIGH;
    if (!gpio_is_ready_dt(&power_ext))
    {
#if LOGS
        LOG_ERR("External power GPIO not ready");
#endif
        return -ENODEV;
    }
    err = gpio_pin_configure_dt(&power_ext, GPIO_OUTPUT_ACTIVE);
    if (err)
    {
#if LOGS
        LOG_ERR("External power config failed (err %d)", err);
#endif
        return err;
    }
#endif
    return 0;
}

// Tap hold functions //

void press_tap_hold(uint8_t idx, uint16_t keycode)
{
    add_held_mod_keys(idx);
    keys[idx].pressed = true;
    keys[idx].debounce_count = 0;
}

void press_delay_tap_hold(uint8_t idx, uint16_t keycode)
{
    if (keys[idx].delay_press_count == tap_hold_p && !keys[idx].held)
    {
        remove_held_mod_keys(idx);
        keycode = keys[idx].tapped
                      ? (uint16_t)(keycode & 0xFF)
                      : (uint16_t)(keycode >> 8);
        release_key(keycode, false);
        int res = press_key(keycode, true);
        if (res != -1)
        {
            keys[idx].held = true;
        }
    }
    else if (keys[idx].delay_press_count < tap_hold_p)
    {
        keys[idx].delay_press_count++;
    }
}

void reset_tap_hold(uint8_t idx)
{
    keys[idx].pressed = false;
    keys[idx].held = false;
    keys[idx].debounce_count = 0;
    keys[idx].delay_press_count = 0;
    keys[idx].delay_release_count = 0;
}

void release_tap_hold(uint8_t idx, uint16_t keycode)
{
    if (keys[idx].held)
    {
        keycode = keys[idx].tapped
                      ? (uint16_t)(keycode & 0xFF)
                      : (uint16_t)(keycode >> 8);
        release_key(keycode, true);
        keys[idx].tapped = false;
        reset_tap_hold(idx);
    }
    else
    {
        remove_held_mod_keys(idx);
        keycode = (uint16_t)(keycode & 0xFF);
        int res = press_key(keycode, true);
        if (res != -1)
        {
            release_key(keycode, true);
            keys[idx].tapped = true;
            reset_tap_hold(idx);
        }
    }
}

void release_delay_tap_hold(uint8_t idx, uint16_t keycode)
{
    if (keys[idx].tapped)
    {
        keys[idx].delay_release_count++;
        if (keys[idx].delay_release_count == tap_hold_r)
        {
            keys[idx].tapped = false;
            keys[idx].delay_release_count = 0;
        }
    }
}

void matrix_scan()
{
    uint8_t idx = 0;
    for (int c = 0; c < MATRIX_COLS; c++)
    {
        /* Drive this column high */
        nrf_gpio_pin_set(abs_cols_pins[c]);

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
            if (nrf_gpio_pin_read(abs_rows_pins[r]))
            {
                if (!keys[idx].pressed)
                {
                    if (keys[idx].debounce_count == debounce_p)
                    {
                        if ((keycode & 0xF000) == K_TAP_HOLD)
                        {
                            press_tap_hold(idx, keycode);
                        }
                        else
                        {
                            release_key(keycode, false);
                            int res = press_key(keycode, false);
                            if (res != -1)
                            {
                                keys[idx].pressed = true;
                                keys[idx].debounce_count = 0;
                            }
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

                    if ((keycode & 0xF000) == K_TAP_HOLD)
                    {
                        press_delay_tap_hold(idx, keycode);
                    }
                }
            }
            else
            {
                if (keys[idx].pressed)
                {
                    if (keys[idx].debounce_count == debounce_r)
                    {
                        if ((keycode & 0xF000) == K_TAP_HOLD)
                        {
                            release_tap_hold(idx, keycode);
                        }
                        else
                        {
                            release_key(keycode, true);
                            keys[idx].pressed = false;
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

                    if ((keycode & 0xF000) == K_TAP_HOLD)
                    {
                        release_delay_tap_hold(idx, keycode);
                    }
                }
            }
        }

        /* Drive column low again */
        nrf_gpio_pin_clear(abs_cols_pins[c]);
    }

#if ENCODERS
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
                press_key(keycode, false);
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

        while (current_conn != NULL && !sent && retries < 200)
        {
            err = send_report(&report_temp);

            if (err == 0)
            {
                sent = true;
            }
            else
            {
                retries++;
                k_msleep(5);
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
#if RGB || POWER_EXT
    k_work_init_delayable(&rgb_power_ext_work, rgb_power_ext_delayer);
#endif
}

int main(void)
{
    int err;

// nrf_power_dcdcen_vddh_set(NRF_POWER, true);
#if defined(CONFIG_SOC_NRF52840_QIAA)
    nrf_power_dcdcen_set(NRF_POWER, true);
#endif

    err = settings_subsys_init();
    if (err)
    {
#if LOGS
        LOG_ERR("Error to init settings subsistem (err %d)", err);
#endif
    }

#if USB
    bmk_check_usb();
    if (usb_connected)
    {
        k_msleep(100);
    }
    usb_hid_dev = device_get_binding("HID_0");
    if (usb_hid_dev)
    {
        usb_hid_register_device(usb_hid_dev, hid_report_map,
                                sizeof(hid_report_map), &usb_ops);
        usb_hid_init(usb_hid_dev);
    }
#endif

    magic_init();
    delayed_init();

    err = bt_enable(NULL);
    if (err)
    {
        return 0;
    }

    bt_conn_auth_cb_register(&auth_cb);
    bt_conn_auth_info_cb_register(&auth_info_cb);

#if IS_ENABLED(CONFIG_SETTINGS)
    settings_load();
    k_msleep(100);
#endif

    init_mac_identity(false);

    debounce_init();
    reports_init();
    keymap_init();

    sleep_init();
    pins_init();
    threads_init();

#if RGB || POWER_EXT
    rgb_power_ext_update();
#if RGB_EFFECTS
    rgb_start_periodic_task();
#endif
#endif

    awake = true;
    last_activity = k_uptime_get_32();

    if (usb_connected)
    {
        k_msleep(1000);
#if LOGS
        LOG_INF("BMK Keyboard started!");
#endif
    }

    start_advertising();

    while (1)
    {
        int32_t uptime = k_uptime_get_32();
        if (((ble_advertising && (uptime - last_activity) > SLEEP_TIMEOUT_ADV_MS) || (!ble_advertising && (uptime - last_activity) > SLEEP_TIMEOUT_MS)) && !some_pressed_key()
#if USB
            && !usb_connected
#endif
        )
        {
            keyboard_sleep();
            while (k_sem_take(&sleep_sem, K_NO_WAIT) == 0)
                ;
            k_sem_take(&sleep_sem, K_FOREVER);
            keyboard_wakeup();
            last_activity = k_uptime_get_32();
        }
#if USB
        else if ((uptime - last_check_usb) > (USB_CHECK_TIMEOUT))
        {
            bmk_check_usb();
            last_check_usb = uptime;
        }
#endif
        matrix_scan();
        k_usleep(CYCLE_DELAY);
    }

    return 0;
}

void k_sys_fatal_error_handler(unsigned int reason, const z_arch_esf_t *esf)
{
#if LOGS
    LOG_ERR("COLD Resetting...");
#endif
    k_msleep(200);
    sys_reboot(SYS_REBOOT_COLD);
    CODE_UNREACHABLE;
}
