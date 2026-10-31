# Copyright (c) 2026 Marcin Bober
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SOC_SERIES_RK3506)
  set(RK_HAL ${CMAKE_CURRENT_LIST_DIR}/rockchip/lib)

  zephyr_library_named(hal_rockchip)

  zephyr_library_include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/soc/rockchip/rk3506
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

  zephyr_library_sources_ifdef(CONFIG_PINCTRL_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/pinctrl/pinctrl_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_UART_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/serial/uart_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_GPIO_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/gpio/gpio_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_SWD_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/misc/swd_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_MBOX_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/mbox/mbox_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_INTC_RK3506_INTMUX ${CMAKE_CURRENT_LIST_DIR}/drivers/interrupt_controller/intc_rk3506_intmux.c)
  zephyr_library_sources_ifdef(CONFIG_COUNTER_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/counter/counter_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_WDT_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/watchdog/wdt_rk3506.c)
  zephyr_library_sources_ifdef(CONFIG_PWM_RK3506 ${CMAKE_CURRENT_LIST_DIR}/drivers/pwm/pwm_rk3506.c)

  if(CONFIG_ADC_RK3506)
    zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/adc)
    zephyr_library_sources(${CMAKE_CURRENT_LIST_DIR}/drivers/adc/adc_rk3506.c)
  endif()
endif()
