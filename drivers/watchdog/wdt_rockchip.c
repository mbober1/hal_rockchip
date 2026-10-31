/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rockchip_wdt

#include "hal_base.h"
#include "hal_cru.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/irq.h>

/* hal_wdt.c keeps the active instance in a global. */
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1,
	     "Only one Rockchip watchdog instance can be enabled");

#define WDT_FREQ           PLL_INPUT_OSC_RATE
/* TOP 15 = 2^31 cycles at 24 MHz; larger values overflow the HAL math. */
#define WDT_MAX_TIMEOUT_MS 89478U

struct wdt_rockchip_config {
	struct WDT_REG *reg;
	uint32_t pclk_gate;
	uint32_t tclk_gate;
	uint32_t psrst;
	uint32_t tsrst;
	uint32_t glb_rst_en;
	unsigned int irq;
	void (*irq_config_func)(void);
};

struct wdt_rockchip_data {
	wdt_callback_t callback;
	uint32_t timeout_ms;
	uint8_t flags;
	bool installed;
	bool started;
};

static void wdt_rockchip_isr(const struct device *dev)
{
	const struct wdt_rockchip_config *cfg = dev->config;
	struct wdt_rockchip_data *data = dev->data;

	if ((HAL_WDT_GetIntStatus() & WDT_STAT_STATUS_MASK) == 0U) {
		return;
	}

	if ((data->flags & WDT_FLAG_RESET_MASK) == WDT_FLAG_RESET_NONE) {
		HAL_WDT_ClearInterrupt();
	} else {
		/* Leave the interrupt pending so the next timeout resets the SoC. */
		irq_disable(cfg->irq);
	}

	if (data->callback) {
		data->callback(dev, 0);
	}
}

static int wdt_rockchip_setup(const struct device *dev, uint8_t options)
{
	const struct wdt_rockchip_config *cfg = dev->config;
	struct wdt_rockchip_data *data = dev->data;
	unsigned int key;

	if (data->started) {
		return -EBUSY;
	}
	if (!data->installed) {
		return -EINVAL;
	}
	if (options & (WDT_OPT_PAUSE_IN_SLEEP | WDT_OPT_PAUSE_HALTED_BY_DBG)) {
		return -ENOTSUP;
	}

	/* GLB_RST_CON has no write-enable mask. */
	key = irq_lock();
	if ((data->flags & WDT_FLAG_RESET_MASK) == WDT_FLAG_RESET_SOC) {
		CRU->GLB_RST_CON |= cfg->glb_rst_en;
	} else {
		CRU->GLB_RST_CON &= ~cfg->glb_rst_en;
	}
	irq_unlock(key);

	HAL_WDT_SetTopMsec(data->timeout_ms, WDT_FREQ);
	if (data->callback) {
		HAL_WDT_ClearInterrupt();
		irq_enable(cfg->irq);
	}
	HAL_WDT_Start(data->callback ? INDIRECT_SYSTEM_RESET : DIRECT_SYSTEM_RESET);
	data->started = true;

	return 0;
}

static int wdt_rockchip_disable(const struct device *dev)
{
	struct wdt_rockchip_data *data = dev->data;

	/* WDT_EN is cleared only by a reset. */
	return data->started ? -EPERM : -EFAULT;
}

static int wdt_rockchip_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{
	struct wdt_rockchip_data *data = dev->data;

	if (data->started) {
		return -EBUSY;
	}
	if (data->installed) {
		return -ENOMEM;
	}
	if (cfg->window.min != 0U || cfg->window.max == 0U ||
	    cfg->window.max > WDT_MAX_TIMEOUT_MS) {
		return -EINVAL;
	}

	switch (cfg->flags & WDT_FLAG_RESET_MASK) {
	case WDT_FLAG_RESET_NONE:
		if (cfg->callback == NULL) {
			return -EINVAL;
		}
		break;
	case WDT_FLAG_RESET_SOC:
		break;
	default:
		return -ENOTSUP;
	}

	data->callback = cfg->callback;
	data->timeout_ms = cfg->window.max;
	data->flags = cfg->flags;
	data->installed = true;

	return 0;
}

static int wdt_rockchip_feed(const struct device *dev, int channel_id)
{
	struct wdt_rockchip_data *data = dev->data;

	if (channel_id != 0 || !data->started) {
		return -EINVAL;
	}

	HAL_WDT_KeepAlive();

	return 0;
}

static DEVICE_API(wdt, wdt_rockchip_api) = {
	.setup = wdt_rockchip_setup,
	.disable = wdt_rockchip_disable,
	.install_timeout = wdt_rockchip_install_timeout,
	.feed = wdt_rockchip_feed,
};

static int wdt_rockchip_init(const struct device *dev)
{
	const struct wdt_rockchip_config *cfg = dev->config;

	HAL_CRU_ClkEnable(cfg->pclk_gate);
	HAL_CRU_ClkEnable(cfg->tclk_gate);
	/* Stops a watchdog left running by previously loaded firmware. */
	HAL_CRU_ClkResetAssert(cfg->psrst);
	HAL_CRU_ClkResetAssert(cfg->tsrst);
	HAL_CRU_ClkResetDeassert(cfg->tsrst);
	HAL_CRU_ClkResetDeassert(cfg->psrst);

	HAL_WDT_Init(WDT_FREQ, cfg->reg);
	cfg->irq_config_func();

	return 0;
}

#define WDT_IS_WDT0(n) (DT_INST_REG_ADDR(n) == WDT0_BASE)

#define WDT_ROCKCHIP_INIT(n)                                                                       \
	BUILD_ASSERT(DT_INST_REG_ADDR(n) == WDT0_BASE || DT_INST_REG_ADDR(n) == WDT1_BASE,         \
		     "Unknown watchdog base");                                                     \
	BUILD_ASSERT(DT_IRQ_LEVEL(DT_DRV_INST(n)) == 1 || IS_ENABLED(CONFIG_INTC_ROCKCHIP_INTMUX), \
		     "This watchdog interrupts through INTMUX: enable &intmux");                   \
	static void wdt_rockchip_irq_config_##n(void)                                                \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), COND_CODE_1(DT_INST_IRQ_HAS_CELL(n, priority),                         \
					(DT_INST_IRQ(n, priority)), (0)),               \
						      wdt_rockchip_isr, DEVICE_DT_INST_GET(n), 0);   \
	}                                                                                          \
	static struct wdt_rockchip_data wdt_rockchip_data_##n;                                         \
	static const struct wdt_rockchip_config wdt_rockchip_cfg_##n = {                               \
		.reg = (struct WDT_REG *)DT_INST_REG_ADDR(n),                                      \
		.pclk_gate = WDT_IS_WDT0(n) ? PCLK_WDT0_GATE : PCLK_WDT1_GATE,                     \
		.tclk_gate = WDT_IS_WDT0(n) ? TCLK_WDT0_GATE : TCLK_WDT1_GATE,                     \
		.psrst = WDT_IS_WDT0(n) ? SRST_PRESETN_WDT0 : SRST_PRESETN_WDT1,                   \
		.tsrst = WDT_IS_WDT0(n) ? SRST_TRESETN_WDT0 : SRST_TRESETN_WDT1,                   \
		.glb_rst_en = WDT_IS_WDT0(n) ? CRU_GLB_RST_CON_CRU_WDT0_EN_MASK                    \
					     : CRU_GLB_RST_CON_CRU_WDT1_EN_MASK,                   \
		.irq = DT_INST_IRQN(n),                                                            \
		.irq_config_func = wdt_rockchip_irq_config_##n,                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, wdt_rockchip_init, NULL, &wdt_rockchip_data_##n, &wdt_rockchip_cfg_##n, \
			      PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &wdt_rockchip_api);

DT_INST_FOREACH_STATUS_OKAY(WDT_ROCKCHIP_INIT)
