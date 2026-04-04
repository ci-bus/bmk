# Copyright (c) 2025 Miguelio
# SPDX-License-Identifier: MIT

board_runner_args(nrfjprog "--nrf-family=NRF52")
board_runner_args(pyocd "--target=nrf52840" "--frequency=4000000")

include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
include(${ZEPHYR_BASE}/boards/common/uf2.board.cmake)
