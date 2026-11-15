This module provides the required glue code (Zephyr abstraction) and `.dtsi` files needed to build a Zephyr application running on Rockchip silicon.

The Rockchip SDK/HAL needed by the drivers is vendored in [rockchip/](rockchip/) and is redistributed here under its original permissive licenses (BSD-3-Clause for Rockchip code, Apache-2.0 for Arm CMSIS). It contains **no proprietary, NDA-covered or binary-only Rockchip packages**; every file in `rockchip/` is provided as source with an explicit license. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the full inventory and the provenance of the import.

## Supported SoCs

- RK3506 (Cortex-M0 coprocessor)

## Already in tree (Zephyr glue present)

- ADC: [adc_rockchip.c](../modules/hal/rockchip/zephyr/drivers/adc/adc_rockchip.c)
- Counter: [counter_rockchip.c](../modules/hal/rockchip/zephyr/drivers/counter/counter_rockchip.c)
- INTMUX: [intc_rockchip_intmux.c](../modules/hal/rockchip/zephyr/drivers/interrupt_controller/intc_rockchip_intmux.c)
- GPIO: [gpio_rockchip.c](../modules/hal/rockchip/zephyr/drivers/gpio/gpio_rockchip.c)
- MBOX: [mbox_rockchip.c](../modules/hal/rockchip/zephyr/drivers/mbox/mbox_rockchip.c)
- PWM: [pwm_rockchip.c](../modules/hal/rockchip/zephyr/drivers/pwm/pwm_rockchip.c)
- Pinctrl: [pinctrl_rockchip.c](../modules/hal/rockchip/zephyr/drivers/pinctrl/pinctrl_rockchip.c)
- SWD: [swd_rockchip.c](../modules/hal/rockchip/zephyr/drivers/misc/swd_rockchip.c)
- UART: [uart_rockchip.c](../modules/hal/rockchip/zephyr/drivers/serial/uart_rockchip.c)
- Watchdog: [wdt_rockchip.c](../modules/hal/rockchip/zephyr/drivers/watchdog/wdt_rockchip.c)

## License

This module is multi-licensed; the applicable license depends on the file.

| Scope | License | Copyright |
|---|---|---|
| Zephyr glue code: `drivers/`, `dts/`, `soc/`, `boards/`, `include/`, `zephyr/`, Kconfig, CMake and top-level files | **Apache-2.0** | Marcin Bober and contributors |
| `rockchip/lib/hal/**`, `rockchip/lib/bsp/**` | **BSD-3-Clause** | Rockchip Electronics Co., Ltd. |
| `rockchip/lib/CMSIS/Device/RK3506/Include/**` | **BSD-3-Clause** | Rockchip Electronics Co., Ltd. |
| `rockchip/lib/CMSIS/LICENSE.txt` | Apache-2.0 license text (retained for provenance) | Arm Limited |

Verbatim license texts live in [LICENSES/](LICENSES/) (`Apache-2.0.txt`, `BSD-3-Clause.txt`); [LICENSE](LICENSE) summarises the scope and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) lists the vendored components with their obligations.

The `rockchip/` tree is a **subset of an unmodified** upstream import: only the
files that the RK3506 port compiles or includes are tracked (37 files). The
remaining 450 files are still present in a local working tree, byte-identical to
upstream, but are listed in [`.gitignore`](.gitignore) and not redistributed —
see [docs/hal_vendor_usage.md](docs/hal_vendor_usage.md) for the resolved
dependency closure and for how to re-enable a peripheral from the pruned
sources. Do not relicense the vendored files, do not strip their copyright
notices, and do not add an Apache-2.0 header to BSD-3-Clause files. Files that
cannot carry an SPDX tag (YAML, Markdown, linker/scatter templates) are mapped in
[REUSE.toml](REUSE.toml).

The module is [REUSE](https://reuse.software/)-compliant — run `reuse lint` to verify.
