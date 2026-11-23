# BMK - BLE Mechanical Keyboard Firmware

Custom USB + BLE HID keyboard firmware built with [Zephyr RTOS](https://zephyrproject.org/) for nRF52840 and nRF52833 boards, plus a USB dongle firmware that bridges a BMK keyboard to USB.

## Features

- **Dual transport**: USB HID when cable connected, BLE HID on battery
- **Auto VBUS detection**: nRF52840 POWER peripheral detects USB cable at startup
- **BLE Boot Protocol**: compatible with macOS, iOS, Windows, Linux, Android
- **BLE advertising**: fast interval after startup and wake-up; after a disconnection, slow interval for 5 minutes and then Bluetooth is turned off until a key is pressed
- **Pairing**: automatic, no PIN required; the bond is cleared with the `HID_BT_CLEAR` key
- **USB dongle**: optional dongle firmware that connects to an unbonded keyboard over BLE and replicates it over USB
- **TX Power**: +8 dBm for maximum Bluetooth range

## Hardware

- **Module**: [E73-2G4M08S1C](https://www.cdebyte.com/) (nRF52840, no external crystal)
- **Clock**: internal RC oscillator (32.768kHz)
- **Power**: VDDH from USB (5V) or battery
- **Bootloader**: Adafruit nRF52 (UF2)
- **Antenna**: integrated ceramic

## Requirements

- [Docker Desktop](https://www.docker.com/products/docker-desktop/) (Mac/Windows/Linux)
- [VS Code](https://code.visualstudio.com/) with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension

## Getting Started

1. **Clone the repository**

   ```bash
   git clone git@github.com:ci-bus/bmk.git
   cd bmk
   ```

2. **Open in VS Code**

   ```bash
   code .
   ```

3. **Reopen in Container**

   VS Code will detect `.devcontainer/` and show a notification — click **"Reopen in Container"**.

   This builds a lightweight Docker image based on `zmkfirmware/zmk-dev-arm:3.5-branch` which includes the Zephyr SDK, ARM toolchain, and all build dependencies.

4. **Initialize the Zephyr workspace** (first time only)

   Once inside the container terminal:

   ```bash
   west init -l app/
   west update
   ```

   This downloads Zephyr and the required modules (hal_nordic, mbedtls, etc.). Docker volumes persist these files, so you only need to do this once.

## Building

Inside the container:

```bash
west build -p auto -b nrf52840 -- -DKEYBOARD=miguelio/mk60v1
```

or

```bash
west build -p auto -b nrf52833 -- -DKEYBOARD=miguelio/mk60v2
```

The firmware output is at `build/zephyr/zephyr.uf2`.

### USB dongle

The dongle firmware runs on the same base hardware as the keyboards (without the key matrix). It connects over BLE to any BMK keyboard that has no bond and replicates its keystrokes over USB:

```bash
west build -p auto -b nrf52840 -d build-dongle -- -DKEYBOARD=dongle
```

The firmware output is at `build-dongle/zephyr/zephyr.uf2`. Use a dedicated build directory for the dongle, as above.

- A keyboard without a bond advertises a BMK "free" mark; the dongle connects to the first free keyboard it sees.
- The link is encrypted but never bonded: neither side stores keys, so the dongle forgets the keyboard when it loses power and the keyboard stays free.
- A keyboard bonded to a computer is ignored by the dongle. Press `HID_BT_CLEAR` on the keyboard to free it.
- To use the keyboard directly with a computer again, unplug the dongle and pair as usual.
- To enable logs over USB serial, set `#define LOGS true` in `app/src/dongle/dongle.h` and build with `-p always`.

## Flashing

1. Double-tap reset to enter bootloader (USB mass storage appears)
2. Drag `build/zephyr/zephyr.uf2` to the drive

Or via command line:

```bash
west flash
```

## Links
[Keycodes](https://docs.zephyrproject.org/latest/doxygen/html/group__usb__hid__mk__report__desc.html)

## Project Structure

```
bmk/
├── .devcontainer/
│   ├── devcontainer.json       # VS Code dev container config
│   ├── Dockerfile              # Dev environment image
│   └── .bashrc                 # Container shell config
├── .vscode/
│   └── tasks.json              # Build/flash tasks
├── app/
│   ├── CMakeLists.txt          # CMake project config (keyboard or dongle target)
│   ├── prj.conf                # Zephyr config (BLE, USB, HID)
│   ├── west.yml                # Zephyr manifest (dependencies)
│   ├── boards/arm/
│   │   ├── common/             # Shared overlays and config (battery, logs, RGB)
│   │   ├── nrf52833/           # nRF52833 board definition
│   │   └── nrf52840/           # nRF52840 board definition
│   ├── keyboards/miguelio/
│   │   ├── mk60v1/             # Pins, options and keymap
│   │   └── mk60v2/
│   └── src/
│       ├── main.c              # Keyboard firmware (matrix, USB HID, BLE HID)
│       ├── main.h              # Keyboard options and defaults
│       ├── hid.h               # Keycodes and HID report map
│       ├── link.h              # BMK "free" advertising mark
│       ├── battery/            # Battery level measurement
│       └── dongle/             # Dongle firmware and its config
```

## Transport Priority

| Startup condition | USB HID | BLE HID | Active transport |
|-------------------|---------|---------|-----------------|
| USB cable connected | Enabled | Enabled | USB (priority) |
| Battery only | Disabled | Enabled | BLE |

## Docker Volumes

The devcontainer uses Docker volumes to persist Zephyr and modules across container rebuilds:

| Volume | Content |
|--------|---------|
| `bmk-zephyr` | Zephyr RTOS source |
| `bmk-modules` | HAL, libraries |
| `bmk-tools` | Build tools |
| `bmk-bootloader` | MCUboot |
| `bmk-root-user` | User cache, west config |

To reset the workspace from scratch:

```bash
docker volume rm bmk-zephyr bmk-modules bmk-tools bmk-bootloader bmk-root-user
```
