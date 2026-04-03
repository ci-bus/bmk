# BMK - BLE Mechanical Keyboard Firmware

Custom BLE keyboard firmware built with [Zephyr RTOS](https://zephyrproject.org/) for nRF52840 and nRF52833.

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
# nRF52840
west build -p auto -b nrf52840dk_nrf52840 app/

# nRF52833
west build -p auto -b nrf52833dk_nrf52833 app/
```

Or use VS Code tasks (`Ctrl+Shift+B`):
- **West Build (nRF52840)** — default build task
- **West Build (nRF52833)**
- **West Build - Clean** — clean rebuild

## Flashing

```bash
west flash
```

> Requires a J-Link or compatible programmer connected to the board.

## Project Structure

```
bmk/
├── .devcontainer/
│   ├── devcontainer.json   # VS Code dev container config
│   ├── Dockerfile          # Dev environment image
│   └── .bashrc             # Container shell config
├── .vscode/
│   └── tasks.json          # Build/flash tasks
├── app/
│   ├── CMakeLists.txt      # CMake project config
│   ├── prj.conf            # Zephyr config (BLE, GPIO, HID)
│   ├── west.yml            # Zephyr manifest (dependencies)
│   └── src/
│       └── main.c          # Firmware entry point
```

## Supported Boards

| Board | Build target |
|-------|-------------|
| nRF52840 DK | `nrf52840dk_nrf52840` |
| nRF52833 DK | `nrf52833dk_nrf52833` |

## Docker Volumes

The devcontainer uses Docker volumes to persist Zephyr and modules across container rebuilds:

| Volume | Path | Content |
|--------|------|---------|
| `bmk-zephyr` | `/workspaces/bmk/zephyr` | Zephyr RTOS source |
| `bmk-modules` | `/workspaces/bmk/modules` | HAL, libraries |
| `bmk-tools` | `/workspaces/bmk/tools` | Build tools |
| `bmk-bootloader` | `/workspaces/bmk/bootloader` | MCUboot |
| `bmk-root-user` | `/root` | User cache, west config |

To reset the workspace from scratch, remove the volumes:

```bash
docker volume rm bmk-zephyr bmk-modules bmk-tools bmk-bootloader bmk-root-user
```
