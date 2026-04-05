/*
 * BMK Keyboard Firmware
 * USB HID + BLE HID with Boot Protocol
 * Priority: USB when connected, BLE otherwise
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <hal/nrf_power.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "matrix.h"

/* Keycodes */
static struct key keys[MATRIX_COLS * MATRIX_ROWS] = {
	// Row 0
	{.kc = HID_KEY_ESC},   // Col 0
	{.kc = 0},			   // Col 1
	{.kc = HID_KEY_2},     // Col 2
	{.kc = HID_KEY_4},     // Col 3
	{.kc = HID_KEY_6},     // Col 4
	{.kc = HID_KEY_8},     // Col 5
	{.kc = HID_KEY_0},     // Col 6
	{.kc = HID_KEY_EQUAL}, // Col 7
	// Row 1
	{.kc = HID_KEY_1},     // Col 0
	{.kc = 0},			   // Col 1
	{.kc = HID_KEY_3},     // Col 2
	{.kc = HID_KEY_5},     // ...
	{.kc = HID_KEY_7},
	{.kc = HID_KEY_9},
	{.kc = HID_KEY_MINUS},
	{.kc = HID_KEY_BACKSPACE},
	// Row 2
	{.kc = HID_KEY_TAB},
	{.kc = HID_KEY_Q},
	{.kc = HID_KEY_E},
	{.kc = HID_KEY_T},
	{.kc = HID_KEY_U},
	{.kc = HID_KEY_O},
	{.kc = HID_KEY_LEFTBRACE},
	{.kc = HID_KEY_BACKSLASH},
	// Row 3
	{.kc = HID_KEY_CAPSLOCK},
	{.kc = 0},
	{.kc = HID_KEY_W},
	{.kc = HID_KEY_R},
	{.kc = HID_KEY_Y},
	{.kc = HID_KEY_I},
	{.kc = HID_KEY_P},
	{.kc = HID_KEY_RIGHTBRACE},
	// Row 4
	{.kc = 0},
	{.kc = HID_KEY_A},
	{.kc = HID_KEY_D},
	{.kc = HID_KEY_G},
	{.kc = HID_KEY_J},
	{.kc = HID_KEY_L},
	{.kc = HID_KEY_APOSTROPHE},
	{.kc = HID_KEY_ENTER},
	// Row 5
	{.kc = 0},
	{.kc = 0},
	{.kc = HID_KEY_S},
	{.kc = HID_KEY_F},
	{.kc = HID_KEY_H},
	{.kc = HID_KEY_K},
	{.kc = HID_KEY_SEMICOLON},
	{.kc = HID_KEY_BACKSLASH},
	// Row 6
	{.kc = 0},
	{.kc = HID_KEY_Z},
	{.kc = HID_KEY_C},
	{.kc = HID_KEY_B},
	{.kc = HID_KEY_M},
	{.kc = HID_KEY_DOT},
	{.kc = HID_KBD_MODIFIER_RIGHT_SHIFT},
	{.kc = HID_KEY_SLASH},
	// Row 7
	{.kc = HID_KBD_MODIFIER_LEFT_SHIFT},
	{.kc = HID_KEY_NUBS},
	{.kc = HID_KEY_X},
	{.kc = HID_KEY_V},
	{.kc = HID_KEY_N},
	{.kc = HID_KEY_COMMA},
	{.kc = HID_KEY_LEFT},
	{.kc = HID_KEY_UP},
	// Row 8
	{.kc = HID_KBD_MODIFIER_LEFT_CTRL},
	{.kc = HID_KBD_MODIFIER_LEFT_UI},
	{.kc = HID_KBD_MODIFIER_LEFT_ALT},
	{.kc = HID_KEY_SPACE},
	{.kc = HID_KBD_MODIFIER_RIGHT_ALT},
	{.kc = HID_KBD_MODIFIER_RIGHT_CTRL},
	{.kc = HID_KEY_DOWN},
	{.kc = HID_KEY_RIGHT}};

LOG_MODULE_REGISTER(bmk, LOG_LEVEL_INF);

/* Matrix GPIO pins from device tree */
#define COL_GPIO_INIT(idx, _) GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, col_gpios, idx),
#define ROW_GPIO_INIT(idx, _) GPIO_DT_SPEC_GET_BY_IDX(MATRIX_NODE, row_gpios, idx),

static const struct gpio_dt_spec cols[] = {
	LISTIFY(MATRIX_COLS, COL_GPIO_INIT, ())
};

static const struct gpio_dt_spec rows[] = {
	LISTIFY(MATRIX_ROWS, ROW_GPIO_INIT, ())
};

static bool changed = false;

/* Advertising parameters: connectable, no timeout */
#define BT_LE_ADV_CONN_FOREVER BT_LE_ADV_PARAM( \
	BT_LE_ADV_OPT_CONNECTABLE,                  \
	BT_GAP_ADV_FAST_INT_MIN_2,                  \
	BT_GAP_ADV_FAST_INT_MAX_2,                  \
	NULL)

/* HID modifier bits */
#define MOD_NONE 0x00
#define MOD_LSHIFT HID_KBD_MODIFIER_LEFT_SHIFT

/* HID Report Descriptor: Standard Keyboard (no Report ID) */
static const uint8_t hid_report_map[] = {
	0x05, 0x01,
	0x09, 0x06,
	0xA1, 0x01,

	/* Modifier keys (8 bits) */
	0x05, 0x07,
	0x19, 0xE0,
	0x29, 0xE7,
	0x15, 0x00,
	0x25, 0x01,
	0x75, 0x01,
	0x95, 0x08,
	0x81, 0x02,

	/* Reserved byte */
	0x75, 0x08,
	0x95, 0x01,
	0x81, 0x01,

	/* LED output (5 bits + 3 padding) */
	0x05, 0x08,
	0x19, 0x01,
	0x29, 0x05,
	0x75, 0x01,
	0x95, 0x05,
	0x91, 0x02,
	0x75, 0x03,
	0x95, 0x01,
	0x91, 0x01,

	/* Key codes (6 bytes) */
	0x05, 0x07,
	0x19, 0x00,
	0x29, 0x65,
	0x15, 0x00,
	0x25, 0x65,
	0x75, 0x08,
	0x95, 0x06,
	0x81, 0x00,

	0xC0};

/* Report buffers */
static uint8_t report[8];
static uint8_t boot_report[8];

/* ===== FUNCTIONS ===== */
static inline bool is_modifier(uint8_t keycode)
{
	return keycode >= 0xE0 && keycode <= 0xE7;
}

static inline uint8_t modifier_bit(uint8_t keycode)
{
	return 1 << (keycode - 0xE0);
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

static int usb_send_report()
{
	int err;
	if (!usb_configured)
	{
		return -ENOTCONN;
	}
	err = hid_int_ep_write(usb_hid_dev, report, sizeof(report), NULL);
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
BT_GATT_SERVICE_DEFINE(hid_svc,
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
											  BT_GATT_PERM_WRITE, NULL, write_ctrl_point, NULL), );

static int ble_send_report()
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
		buf = report;
		attr = &hid_svc.attrs[6];
	}
	else
	{
		return -ENOTCONN;
	}
	err = bt_gatt_notify(current_conn, attr, report, 8);
	return err;
}

/* ==================== Connection Management ==================== */

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
				  BT_UUID_16_ENCODE(BT_UUID_HIDS_VAL),
				  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL),
				  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, 0xC1, 0x03),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
			sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_advertising(void)
{
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FOREVER, ad, ARRAY_SIZE(ad),
							  sd, ARRAY_SIZE(sd));
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

static int press_key(uint8_t keycode)
{
	uint8_t idx = 0;
	if (is_modifier(keycode))
	{
		report[0] |= modifier_bit(keycode);
	}
	else
	{
		// Find free index to send keycode
		for (uint8_t i = 2; i < 8; i++)
		{
			if (report[i] == 0)
				idx = i;
		}
		if (idx != 0)
		{
			// Press key
			report[idx] = keycode;
		}
		else
		{
			// No free index
			return -1;
		}
	}
	return idx;
}

static int release_key(uint8_t keycode)
{
	uint8_t idx = 0;
	if (is_modifier(keycode))
	{
		report[0] &= ~modifier_bit(keycode);
	}
	else
	{
		// Find pressed keycode
		for (uint8_t i = 2; i < 8; i++)
		{
			if (report[i] == keycode)
			{
				// Release key
				report[i] = 0;
				idx = i;
			}
		}
		if (idx == 0)
		{
			// Key is not pressed
			return -1;
		}
	}
	return idx;
}

static int send_report()
{
	/* Priority: USB if configured, otherwise BLE */
	if (usb_configured)
	{
		return usb_send_report();
	}
	return ble_send_report();
}

/* ===== MATRIX ===== */

int matrix_init(void)
{
	int err;

	/* Configure columns as output, initially low */
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

	/* Configure rows as input with pull-down */
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

	LOG_INF("Matrix initialized: %d cols x %d rows", MATRIX_COLS, MATRIX_ROWS);
	return 0;
}

void matrix_scan()
{
	uint8_t idx = 0;
	changed = false;
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
			if (!keys[idx].kc)
				continue;
			if (gpio_pin_get_dt(&rows[r]))
			{
				if (!keys[idx].pressed)
				{
					if (keys[idx].debounce_count > DEBOUNCE_PRESS)
					{
						release_key(keys[idx].kc); // Hold key down once
						int res = press_key(keys[idx].kc);
						if (res != -1)
						{
							keys[idx].pressed = true;
							keys[idx].debounce_count = 0;
							changed = true;
						}
					}
					else
					{
						keys[idx].debounce_count++;
					}
				}
				else
				{
					if (keys[idx].debounce_count > 0)
					{
						keys[idx].debounce_count--;
					}
				}
			}
			else
			{
				if (keys[idx].pressed)
				{
					if (keys[idx].debounce_count > DEBOUNCE_RELEASE)
					{
						int res = release_key(keys[idx].kc);
						if (res != -1)
						{
							keys[idx].pressed = false;
							keys[idx].debounce_count = 0;
							changed = true;
						}
					}
					else
					{
						keys[idx].debounce_count++;
					}
				}
				else
				{
					if (keys[idx].debounce_count > 0)
					{
						keys[idx].debounce_count--;
					}
				}
			}
			// TODO set pressed or released key with debounce logic
		}

		/* Drive column low again */
		gpio_pin_set_dt(&cols[c], 0);
	}
	if (changed)
	{
		send_report();
	}
}

/* ==================== Main ==================== */

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

	matrix_init();

	while (1)
	{
		matrix_scan();

		k_sleep(K_MSEC(1));
	}

	return 0;
}
