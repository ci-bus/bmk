/*
 * BMK Keyboard Firmware
 * BLE HID + USB HID keyboard that types "Hello World"
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

/* "Hello World" keycodes: {modifier, keycode} */
static const uint8_t hello_world[][2] = {
	{ 0x02, 0x0B }, /* H (shift+h) */
	{ 0x00, 0x08 }, /* e */
	{ 0x00, 0x0F }, /* l */
	{ 0x00, 0x0F }, /* l */
	{ 0x00, 0x12 }, /* o */
	{ 0x00, 0x2C }, /* space */
	{ 0x02, 0x1A }, /* W (shift+w) */
	{ 0x00, 0x12 }, /* o */
	{ 0x00, 0x15 }, /* r */
	{ 0x00, 0x0F }, /* l */
	{ 0x00, 0x07 }, /* d */
};

/* HID Report Map: Standard Keyboard (no Report ID) */
static const uint8_t hid_report_map[] = {
	0x05, 0x01,       /* Usage Page (Generic Desktop) */
	0x09, 0x06,       /* Usage (Keyboard) */
	0xA1, 0x01,       /* Collection (Application) */

	/* Modifier keys (8 bits) */
	0x05, 0x07,       /*   Usage Page (Keyboard/Keypad) */
	0x19, 0xE0,       /*   Usage Minimum (Left Control) */
	0x29, 0xE7,       /*   Usage Maximum (Right GUI) */
	0x15, 0x00,       /*   Logical Minimum (0) */
	0x25, 0x01,       /*   Logical Maximum (1) */
	0x75, 0x01,       /*   Report Size (1) */
	0x95, 0x08,       /*   Report Count (8) */
	0x81, 0x02,       /*   Input (Data, Variable, Absolute) */

	/* Reserved byte */
	0x75, 0x08,       /*   Report Size (8) */
	0x95, 0x01,       /*   Report Count (1) */
	0x81, 0x01,       /*   Input (Constant) */

	/* Key codes (6 bytes) */
	0x05, 0x07,       /*   Usage Page (Keyboard/Keypad) */
	0x19, 0x00,       /*   Usage Minimum (0) */
	0x29, 0x65,       /*   Usage Maximum (101) */
	0x15, 0x00,       /*   Logical Minimum (0) */
	0x25, 0x65,       /*   Logical Maximum (101) */
	0x75, 0x08,       /*   Report Size (8) */
	0x95, 0x06,       /*   Report Count (6) */
	0x81, 0x00,       /*   Input (Data, Array) */

	0xC0              /* End Collection */
};

/* Report buffer: modifier(1) + reserved(1) + keys(6) = 8 bytes */
static uint8_t report[8];

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
		usb_configured = false;
		LOG_INF("USB disconnected");
		break;
	default:
		break;
	}
}

static void usb_int_in_ready(const struct device *dev)
{
	/* EP ready for next report */
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
static struct bt_conn *current_conn;

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

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ble_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("BLE HID notifications %s", ble_notify_enabled ? "enabled" : "disabled");
}

static ssize_t read_input_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				     void *buf, uint16_t len, uint16_t offset)
{
	static const uint8_t ref[] = { 0x00, 0x01 };
	return bt_gatt_attr_read(conn, attr, buf, len, offset, ref, sizeof(ref));
}

static ssize_t read_protocol_mode(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  void *buf, uint16_t len, uint16_t offset)
{
	static uint8_t mode = 0x01;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &mode, sizeof(mode));
}

BT_GATT_SERVICE_DEFINE(hid_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_hid_info, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, read_report_map, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_report, NULL, NULL),
	BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ,
			   read_input_report_ref, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_PROTOCOL_MODE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_protocol_mode, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE, NULL, NULL, NULL),
);

static int ble_send_key(uint8_t modifier, uint8_t keycode)
{
	int err;

	if (!current_conn || !ble_notify_enabled) {
		return -ENOTCONN;
	}

	memset(report, 0, sizeof(report));
	report[0] = modifier;
	report[2] = keycode;
	err = bt_gatt_notify(current_conn, &hid_svc.attrs[7], report, sizeof(report));
	if (err) {
		return err;
	}
	k_sleep(K_MSEC(50));

	memset(report, 0, sizeof(report));
	err = bt_gatt_notify(current_conn, &hid_svc.attrs[7], report, sizeof(report));
	k_sleep(K_MSEC(50));

	return err;
}

/* Advertising data */
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

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("BLE connection failed (err 0x%02x)", err);
		return;
	}
	LOG_INF("BLE connected");
	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("BLE disconnected (reason 0x%02x)", reason);
	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	ble_notify_enabled = false;

	/* Restart advertising after disconnect */
	bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	LOG_INF("Re-advertising...");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

/* ==================== Type Hello World ==================== */

static void type_hello_world(int (*send_fn)(uint8_t, uint8_t), const char *transport)
{
	LOG_INF("Typing 'Hello World' via %s...", transport);
	for (size_t i = 0; i < ARRAY_SIZE(hello_world); i++) {
		if (send_fn(hello_world[i][0], hello_world[i][1])) {
			LOG_ERR("Failed at key %zu", i);
			return;
		}
	}
	LOG_INF("%s: done", transport);
}

/* ==================== Main ==================== */

int main(void)
{
	int err;

	LOG_INF("BMK Keyboard starting...");

	/* BLE init first - must work with or without USB */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}
	LOG_INF("Bluetooth ready");

	/* Clear all bonds on startup for clean pairing */
	bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
	LOG_INF("All bonds cleared");

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed (err %d)", err);
		return 0;
	}
	LOG_INF("Advertising as '%s'", CONFIG_BT_DEVICE_NAME);

	/* USB HID init - only if VBUS is detected (cable plugged in) */
	if (nrf_power_usbregstatus_vbusdet_get(NRF_POWER)) {
		LOG_INF("VBUS detected, initializing USB HID");
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
		LOG_INF("No VBUS - battery mode, BLE only");
	}

	while (1) {
		/* USB: send key every 5 seconds */
		if (usb_configured) {
			type_hello_world(usb_send_key, "USB");
		}

		/* BLE: send key every 5 seconds */
		if (current_conn && ble_notify_enabled) {
			type_hello_world(ble_send_key, "BLE");
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
