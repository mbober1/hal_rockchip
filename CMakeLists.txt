# Copyright (c) 2026 Marcin Bober
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_SERIES_RK35)
  set(RK_HAL ${CMAKE_CURRENT_LIST_DIR}/rockchip/lib)

  zephyr_library_named(hal_rockchip)

  zephyr_library_include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/soc/rockchip/rk35/rk3506
    ${RK_HAL}/hal/inc
    ${RK_HAL}/bsp/RK3506
    ${RK_HAL}/CMSIS/Device/RK3506/Include
  )

  zephyr_library_sources(
    ${RK_HAL}/hal/src/hal_gpio.c
    ${RK_HAL}/hal/src/hal_mbox.c
    ${RK_HAL}/hal/src/hal_pwm.c
    ${RK_HAL}/hal/src/hal_saradc.c
    ${RK_HAL}/hal/src/hal_uart.c
    ${RK_HAL}/hal/src/hal_wdt.c
    ${RK_HAL}/hal/src/pinctrl/hal_pinctrl.c
    ${RK_HAL}/hal/src/pinctrl/hal_pinctrl_rk3506.c
    ${RK_HAL}/hal/src/cru/hal_cru.c
    ${RK_HAL}/hal/src/cru/hal_cru_rk3506.c
    ${RK_HAL}/bsp/RK3506/hal_bsp.c
  )

  zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/include)

  zephyr_library_sources_ifdef(CONFIG_PINCTRL_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/pinctrl/pinctrl_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_UART_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/serial/uart_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_GPIO_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/gpio/gpio_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_SWD_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/misc/swd_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_MBOX_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/mbox/mbox_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_INTC_ROCKCHIP_INTMUX ${CMAKE_CURRENT_LIST_DIR}/drivers/interrupt_controller/intc_rockchip_intmux.c)
  zephyr_library_sources_ifdef(CONFIG_COUNTER_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/counter/counter_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_WDT_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/watchdog/wdt_rockchip.c)
  zephyr_library_sources_ifdef(CONFIG_PWM_ROCKCHIP ${CMAKE_CURRENT_LIST_DIR}/drivers/pwm/pwm_rockchip.c)

  if(CONFIG_ADC_ROCKCHIP)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/adc)
    zephyr_library_sources(${CMAKE_CURRENT_LIST_DIR}/drivers/adc/adc_rockchip.c)
  endif()
endif()
