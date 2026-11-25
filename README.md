# BMK - Bluetooth Mechanical Keyboard firmware

USB and Bluetooth Low Energy keyboard firmware built with [Zephyr RTOS](https://zephyrproject.org/) for nRF52840 and nRF52833 boards, plus a USB dongle firmware that bridges a keyboard to USB.

## Features

- USB HID when the cable is connected, Bluetooth LE HID on battery
- Layers, tap-hold keys, rotary encoder and media keys
- RGB lighting with WS2812 LEDs
- Battery level reported to the host
- Sleep and deep sleep, with wake-up on key press
- Gaming mode with a faster matrix scan
- USB dongle that presents a wireless keyboard as a wired one
- UF2 flashing

## Documentation

The complete documentation lives in the `documentation` folder. Clone the repository and open `documentation/index.html` in a browser.

It covers installation, building, flashing, keymaps and keycodes, every feature, all the configuration options, how to add a new keyboard and troubleshooting.

## Quick start

You need [Docker Desktop](https://www.docker.com/products/docker-desktop/) and [VS Code](https://code.visualstudio.com/) with the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension.

```bash
git clone git@github.com:ci-bus/bmk.git
cd bmk
code .
```

Choose **Reopen in Container** in VS Code and, the first time only, download Zephyr:

```bash
west init -l app/
west update
```

Build a keyboard:

```bash
cd app
west build -p auto -b nrf52840 -- -DKEYBOARD=miguelio/mk60v1
```

Flash it: double-tap the reset button and copy `app/build/zephyr/zephyr.uf2` to the USB drive that appears.

## Keyboards

| Keyboard | Board |
|----------|-------|
| `miguelio/mk60v1` | `nrf52840` |
| `miguelio/mk60v2` | `nrf52833` |
| `miguelio/aether75` | `nrf52840` |

## USB dongle

```bash
west build -p auto -b nrf52840 -d build-dongle -- -DKEYBOARD=dongle
```

The dongle connects over Bluetooth to a keyboard that has no paired host and replicates its keys over USB. See the documentation for details.
