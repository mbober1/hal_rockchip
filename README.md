This module provides the required glue code (Zephyr abstraction) and `.dtsi` files needed to build a Zephyr application running on Rockchip silicon. **It not contains any Rockchip proparetiary SDK files and packages.**

You must manually download a compatible SDK if you wish to use it.

## Already in tree (Zephyr glue present)

- ADC: [adc_rk3506.c](../modules/hal/rockchip/zephyr/drivers/adc/adc_rk3506.c)
- Counter: [counter_rk3506.c](../modules/hal/rockchip/zephyr/drivers/counter/counter_rk3506.c)
- INTMUX: [intc_rk3506_intmux.c](../modules/hal/rockchip/zephyr/drivers/interrupt_controller/intc_rk3506_intmux.c)
- GPIO: [gpio_rk3506.c](../modules/hal/rockchip/zephyr/drivers/gpio/gpio_rk3506.c)
- MBOX: [mbox_rk3506.c](../modules/hal/rockchip/zephyr/drivers/mbox/mbox_rk3506.c)
- PWM: [pwm_rk3506.c](../modules/hal/rockchip/zephyr/drivers/pwm/pwm_rk3506.c)
- Pinctrl: [pinctrl_rk3506.c](../modules/hal/rockchip/zephyr/drivers/pinctrl/pinctrl_rk3506.c)
- SWD: [swd_rk3506.c](../modules/hal/rockchip/zephyr/drivers/misc/swd_rk3506.c)
- UART: [uart_rk3506.c](../modules/hal/rockchip/zephyr/drivers/serial/uart_rk3506.c)
- Watchdog: [wdt_rk3506.c](../modules/hal/rockchip/zephyr/drivers/watchdog/wdt_rk3506.c)

## HAL present, no Zephyr driver yet

| Priority | Peripheral | Base (CMSIS) | HAL source | Notes |
|---|---|---|---|---|
| High | I2C (x3) | 0xFF040000-0xFF060000 | [hal_i2c.c](../modules/hal/rockchip/lib/hal/src/hal_i2c.c) | Zephyr `i2c` API |
| High | SPI (SPI0/1) | 0xFF120000 / 0xFF130000 | [hal_spi.c](../modules/hal/rockchip/lib/hal/src/hal_spi.c) | Zephyr `spi` API. SPI2 via [hal_spi2apb.c](../modules/hal/rockchip/lib/hal/src/hal_spi2apb.c) / [hal_spi2ahb.c](../modules/hal/rockchip/lib/hal/src/hal_spi2ahb.c) |
| High | Random (TRNG0/1) | 0xFF710000 / 0xFFFE0000 | [hal_trng.c](../modules/hal/rockchip/lib/hal/src/hal_trng.c) | RK3506-guarded. Zephyr `entropy` API |
| Medium | TSADC (temperature) | 0xFF650000 | [hal_tsadc.c](../modules/hal/rockchip/lib/hal/src/hal_tsadc.c) | RK3506 branch exists. Zephyr `sensor` API |
| Medium | Crypto (AES/HASH/PKA) | 0xFF700000 | [hal_crypto.c](../modules/hal/rockchip/lib/hal/src/crypto/hal_crypto.c), [hal_crypto_pka.c](../modules/hal/rockchip/lib/hal/src/crypto/hal_crypto_pka.c) | Zephyr `crypto` API. Check secure access from M0 first |
| Medium | OTP / eFuse | | [hal_otp.c](../modules/hal/rockchip/lib/hal/src/hal_otp.c), [hal_efuse.c](../modules/hal/rockchip/lib/hal/src/hal_efuse.c) | Read-only use. Secure OTP may be blocked |
| Medium | Hardware spinlock | 0xFF240000-0xFF243000 | [hal_hwspinlock.c](../modules/hal/rockchip/lib/hal/src/hal_hwspinlock.c) | Needed if A7 and M0 share peripherals |
| Medium | DMA (DMA0/1) | 0xFF000000 / 0xFF008000 | [hal_dwdma.c](../modules/hal/rockchip/lib/hal/src/hal_dwdma.c) | Zephyr `dma` API |
| Low | CAN FD (x2) | 0xFF320000 / 0xFF330000 | [hal_canfd.c](../modules/hal/rockchip/lib/hal/src/hal_canfd.c) | RK3506 branch exists. Zephyr `can` API |
| Low | SAI (x5), I2S | 0xFF300000 and others | [hal_sai.c](../modules/hal/rockchip/lib/hal/src/hal_sai.c), [hal_i2s.c](../modules/hal/rockchip/lib/hal/src/hal_i2s.c) | Audio. Zephyr `i2s` API |
| Low | PDM (8 ch) | 0xFF380000 | [hal_pdm.c](../modules/hal/rockchip/lib/hal/src/hal_pdm.c) | Audio capture |
| Low | Audio ACODEC / DSM ADC | 0xFF4F8000 / 0xFF4B0000 | [hal_acodec_rk3506.c](../modules/hal/rockchip/lib/hal/src/codec/hal_acodec_rk3506.c), [hal_acdcdig_dsm.c](../modules/hal/rockchip/lib/hal/src/hal_acdcdig_dsm.c) | RK3506-specific |
| Low | FLEXBUS (parallel GPIO) | 0xFF880000 | [hal_flexbus_rk3506.c](../modules/hal/rockchip/lib/hal/src/flexbus/hal_flexbus_rk3506.c), [hal_flexbus_adc.c](../modules/hal/rockchip/lib/hal/src/flexbus/hal_flexbus_adc.c), [hal_flexbus_dac.c](../modules/hal/rockchip/lib/hal/src/flexbus/hal_flexbus_dac.c), [hal_flexbus_spi.c](../modules/hal/rockchip/lib/hal/src/flexbus/hal_flexbus_spi.c) | RK3506-specific |
| Low | NPOR | | [hal_npor_rk3506.c](../modules/hal/rockchip/lib/hal/src/npor/hal_npor_rk3506.c) | Power-on reset |
