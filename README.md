# BMK - BLE Mechanical Keyboard Firmware

Custom USB + BLE HID keyboard firmware built with [Zephyr RTOS](https://zephyrproject.org/) for the E73-2G4M08S1C module (nRF52840).

## Features

- **Dual transport**: USB HID when cable connected, BLE HID on battery
- **Auto VBUS detection**: nRF52840 POWER peripheral detects USB cable at startup
- **BLE Boot Protocol**: compatible with macOS, iOS, Windows, Linux, Android
- **BLE advertising**: no timeout, fast interval, auto re-advertise on disconnect
- **Pairing**: automatic, no PIN required, bonds cleared on startup
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

The firmware output is at `build/zephyr/zephyr.uf2`.

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
│   ├── CMakeLists.txt          # CMake project config
│   ├── prj.conf                # Zephyr config (BLE, USB, HID)
│   ├── west.yml                # Zephyr manifest (dependencies)
│   ├── boards/
│   │   └── arm/bmk_board/      # Custom board for E73-2G4M08S1C
│   │       ├── bmk_board.dts   # Device tree (flash partitions, USB, GPIO)
│   │       ├── bmk_board_defconfig  # Board defaults (clock, flash, NVS)
│   │       ├── Kconfig.board   # Board Kconfig definition
│   │       └── Kconfig.defconfig    # Board Kconfig defaults
│   └── src/
│       └── main.c              # Firmware (USB HID + BLE HID + GATT)
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
