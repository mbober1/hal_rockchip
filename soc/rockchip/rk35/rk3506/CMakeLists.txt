# Copyright (c) 2026 Marcin Bober
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ARM)
  set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/arm/cortex_m/scripts/linker.ld CACHE INTERNAL "SoC Linker script")
endif()

zephyr_include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/../../../../rockchip/lib/CMSIS/Device/RK3506/Include
)

zephyr_library_sources(soc.c)

zephyr_linker_sources_ifdef(CONFIG_OPENAMP_RSC_TABLE SECTIONS rsc_table.ld)
