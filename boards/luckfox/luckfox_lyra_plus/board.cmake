# Copyright (c) 2026 Marcin Bober
# SPDX-License-Identifier: Apache-2.0

board_runner_args(jlink "--device=Cortex-M0")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
