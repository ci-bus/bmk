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

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Advertising parameters: connectable, no timeout */
#define BT_LE_ADV_CONN_FOREVER BT_LE_ADV_PARAM( \
	BT_LE_ADV_OPT_CONNECTABLE, \
	BT_GAP_ADV_FAST_INT_MIN_2, \
	BT_GAP_ADV_FAST_INT_MAX_2, \
	NULL)

/* "Hello World" keycodes: {modifier, keycode} */
static const uint8_t hello_world[][2] = {
	{ 0x02, 0x0B }, /* H */
	{ 0x00, 0x08 }, /* e */
	{ 0x00, 0x0F }, /* l */
	{ 0x00, 0x0F }, /* l */
	{ 0x00, 0x12 }, /* o */
	{ 0x00, 0x2C }, /* space */
	{ 0x02, 0x1A }, /* W */
	{ 0x00, 0x12 }, /* o */
	{ 0x00, 0x15 }, /* r */
	{ 0x00, 0x0F }, /* l */
	{ 0x00, 0x07 }, /* d */
};

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

	0xC0
};

/* Report buffers */
static uint8_t report[8];
static uint8_t boot_report[8];

/* ==================== USB HID ==================== */

static const struct device *usb_hid_dev;
static volatile bool usb_configured;

static void usb_status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	switch (status) {
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

static int usb_send_key(uint8_t modifier, uint8_t keycode)
{
	int err;

	if (!usb_configured) {
		return -ENOTCONN;
	}

	/* Key press */
	memset(report, 0, sizeof(report));
	report[0] = modifier;
	report[2] = keycode;
	err = hid_int_ep_write(usb_hid_dev, report, sizeof(report), NULL);
	if (err) {
		return err;
	}
	k_sleep(K_MSEC(50));

	/* Key release */
	memset(report, 0, sizeof(report));
	err = hid_int_ep_write(usb_hid_dev, report, sizeof(report), NULL);
	k_sleep(K_MSEC(50));

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
	static const uint8_t info[] = { 0x11, 0x01, 0x00, 0x02 };
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
	static const uint8_t ref[] = { 0x00, 0x01 };
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
	if (len != 1 || offset != 0) {
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
			       BT_GATT_PERM_WRITE, NULL, write_ctrl_point, NULL),
);

static int ble_send_key(uint8_t modifier, uint8_t keycode)
{
	int err;
	uint8_t *buf;
	const struct bt_gatt_attr *attr;

	if (!current_conn) {
		return -ENOTCONN;
	}

	if (protocol_mode == 0x00 && boot_notify_enabled) {
		buf = boot_report;
		attr = &hid_svc.attrs[10];
	} else if (ble_notify_enabled) {
		buf = report;
		attr = &hid_svc.attrs[6];
	} else {
		return -ENOTCONN;
	}

	memset(buf, 0, 8);
	buf[0] = modifier;
	buf[2] = keycode;
	err = bt_gatt_notify(current_conn, attr, buf, 8);
	if (err) {
		return err;
	}
	k_sleep(K_MSEC(50));

	memset(buf, 0, 8);
	err = bt_gatt_notify(current_conn, attr, buf, 8);
	k_sleep(K_MSEC(50));

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
	if (err && err != -EALREADY) {
		LOG_ERR("Advertising failed (err %d)", err);
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
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
	if (current_conn) {
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
	if (err) {
		LOG_ERR("Security failed (err %d)", err);
	} else {
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

/* ==================== Send Key (auto-selects transport) ==================== */

static int send_key(uint8_t modifier, uint8_t keycode)
{
	/* Priority: USB if configured, otherwise BLE */
	if (usb_configured) {
		return usb_send_key(modifier, keycode);
	}
	return ble_send_key(modifier, keycode);
}

/* ==================== Main ==================== */

int main(void)
{
	int err;

	LOG_INF("BMK Keyboard starting...");

	/* BLE init first -- always available */
	err = bt_enable(NULL);
	if (err) {
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
	if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
		LOG_INF("VBUS detected, initializing USB");
		usb_hid_dev = device_get_binding("HID_0");
		if (usb_hid_dev) {
			usb_hid_register_device(usb_hid_dev, hid_report_map,
						sizeof(hid_report_map), &usb_ops);
			usb_hid_init(usb_hid_dev);
			err = usb_enable(usb_status_cb);
			if (err) {
				LOG_WRN("USB enable failed (err %d)", err);
			} else {
				LOG_INF("USB HID ready");
			}
		}
	} else {
		LOG_INF("No VBUS -- battery mode, BLE only");
	}

	while (1) {
		for (size_t i = 0; i < ARRAY_SIZE(hello_world); i++) {
			if (send_key(hello_world[i][0], hello_world[i][1])) {
				break;
			}
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
