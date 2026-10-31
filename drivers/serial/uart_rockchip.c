/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rockchip_uart

#include "hal_bsp.h"
#include "hal_cru.h"
#include "hal_uart.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

struct uart_rockchip_config {
	const struct HAL_UART_DEV *hal;
	const struct pinctrl_dev_config *pcfg;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	void (*irq_config_func)(const struct device *dev);
#endif
};

struct uart_rockchip_data {
	struct uart_config uart_cfg;
	struct k_spinlock lock;
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_callback_user_data_t cb;
	void *cb_data;
	uint32_t iir;
#endif
};

static inline struct UART_REG *uart_rockchip_regs(const struct device *dev)
{
	const struct uart_rockchip_config *config = dev->config;

	return config->hal->pReg;
}

static int uart_rockchip_apply(const struct device *dev, const struct uart_config *cfg)
{
	const struct uart_rockchip_config *config = dev->config;
	struct HAL_UART_CONFIG hal_cfg;

	if (cfg->baudrate == 0U) {
		return -EINVAL;
	}

	switch (cfg->parity) {
	case UART_CFG_PARITY_NONE:
		hal_cfg.parity = UART_PARITY_DISABLE;
		break;
	case UART_CFG_PARITY_ODD:
		hal_cfg.parity = UART_ODD_PARITY;
		break;
	case UART_CFG_PARITY_EVEN:
		hal_cfg.parity = UART_EVEN_PARITY;
		break;
	default:
		return -ENOTSUP;
	}

	if (cfg->data_bits > UART_CFG_DATA_BITS_8) {
		return -ENOTSUP;
	}
	hal_cfg.dataBit = (eUART_dataLen)(UART_DATA_5B + cfg->data_bits);

	/* One LCR.STOP bit: 1.5 stop bits with 5 data bits, 2 otherwise. */
	switch (cfg->stop_bits) {
	case UART_CFG_STOP_BITS_1:
		hal_cfg.stopBit = UART_ONE_STOPBIT;
		break;
	case UART_CFG_STOP_BITS_1_5:
		if (cfg->data_bits != UART_CFG_DATA_BITS_5) {
			return -ENOTSUP;
		}
		hal_cfg.stopBit = UART_ONE_AND_HALF_OR_TWO_STOPBIT;
		break;
	case UART_CFG_STOP_BITS_2:
		if (cfg->data_bits == UART_CFG_DATA_BITS_5) {
			return -ENOTSUP;
		}
		hal_cfg.stopBit = UART_ONE_AND_HALF_OR_TWO_STOPBIT;
		break;
	default:
		return -ENOTSUP;
	}

	if (cfg->flow_ctrl != UART_CFG_FLOW_CTRL_NONE) {
		return -ENOTSUP;
	}

	hal_cfg.baudRate = (eUART_baudRate)cfg->baudrate;

	/* The DW UART ignores LCR/DLL writes while it is busy. */
	while (!(config->hal->pReg->LSR & UART_LSR_TEMT)) {
	}

	if (HAL_UART_Init(config->hal, &hal_cfg) != HAL_OK) {
		return -EIO;
	}

	return 0;
}

static int uart_rockchip_poll_in(const struct device *dev, unsigned char *c)
{
	struct UART_REG *regs = uart_rockchip_regs(dev);

	if (!(regs->LSR & UART_LSR_DR)) {
		return -1;
	}

	*c = (unsigned char)regs->RBR;

	return 0;
}

static void uart_rockchip_poll_out(const struct device *dev, unsigned char c)
{
	struct uart_rockchip_data *data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	HAL_UART_SerialOutChar(uart_rockchip_regs(dev), (char)c);

	k_spin_unlock(&data->lock, key);
}

static int uart_rockchip_err_check(const struct device *dev)
{
	uint32_t lsr = uart_rockchip_regs(dev)->LSR;
	int err = 0;

	if (lsr & UART_LSR_OE) {
		err |= UART_ERROR_OVERRUN;
	}
	if (lsr & UART_LSR_PE) {
		err |= UART_ERROR_PARITY;
	}
	if (lsr & UART_LSR_FE) {
		err |= UART_ERROR_FRAMING;
	}
	if (lsr & UART_LSR_BI) {
		err |= UART_BREAK;
	}

	return err;
}

#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
static int uart_rockchip_configure(const struct device *dev, const struct uart_config *cfg)
{
	struct uart_rockchip_data *data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&data->lock);
	int ret;

	ret = uart_rockchip_apply(dev, cfg);
	if (ret == 0) {
		data->uart_cfg = *cfg;
	}

	k_spin_unlock(&data->lock, key);

	return ret;
}

static int uart_rockchip_config_get(const struct device *dev, struct uart_config *cfg)
{
	struct uart_rockchip_data *data = dev->data;

	*cfg = data->uart_cfg;

	return 0;
}
#endif /* CONFIG_UART_USE_RUNTIME_CONFIGURE */

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static int uart_rockchip_fifo_fill(const struct device *dev, const uint8_t *tx_data, int size)
{
	struct UART_REG *regs = uart_rockchip_regs(dev);
	int i;

	for (i = 0; i < size && (regs->USR & UART_USR_TX_FIFO_NOT_FULL); i++) {
		regs->THR = tx_data[i];
	}

	return i;
}

static int uart_rockchip_fifo_read(const struct device *dev, uint8_t *rx_data, const int size)
{
	struct UART_REG *regs = uart_rockchip_regs(dev);
	int i;

	for (i = 0; i < size && (regs->LSR & UART_LSR_DR); i++) {
		rx_data[i] = (uint8_t)regs->RBR;
	}

	return i;
}

static void uart_rockchip_ier_update(const struct device *dev, uint32_t bits, bool enable)
{
	struct uart_rockchip_data *data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	if (enable) {
		HAL_UART_EnableIrq(uart_rockchip_regs(dev), bits);
	} else {
		HAL_UART_DisableIrq(uart_rockchip_regs(dev), bits);
	}

	k_spin_unlock(&data->lock, key);
}

static void uart_rockchip_irq_tx_enable(const struct device *dev)
{
	uart_rockchip_ier_update(dev, UART_IER_THRI, true);
}

static void uart_rockchip_irq_tx_disable(const struct device *dev)
{
	uart_rockchip_ier_update(dev, UART_IER_THRI, false);
}

static int uart_rockchip_irq_tx_ready(const struct device *dev)
{
	struct UART_REG *regs = uart_rockchip_regs(dev);

	return (regs->IER & UART_IER_THRI) && (regs->USR & UART_USR_TX_FIFO_NOT_FULL);
}

static int uart_rockchip_irq_tx_complete(const struct device *dev)
{
	return (uart_rockchip_regs(dev)->LSR & UART_LSR_TEMT) != 0U;
}

static void uart_rockchip_irq_rx_enable(const struct device *dev)
{
	uart_rockchip_ier_update(dev, UART_IER_RDI, true);
}

static void uart_rockchip_irq_rx_disable(const struct device *dev)
{
	uart_rockchip_ier_update(dev, UART_IER_RDI, false);
}

static int uart_rockchip_irq_rx_ready(const struct device *dev)
{
	return (uart_rockchip_regs(dev)->LSR & UART_LSR_DR) != 0U;
}

static void uart_rockchip_irq_err_enable(const struct device *dev)
{
	uart_rockchip_ier_update(dev, UART_IER_RLSI, true);
}

static void uart_rockchip_irq_err_disable(const struct device *dev)
{
	uart_rockchip_ier_update(dev, UART_IER_RLSI, false);
}

static int uart_rockchip_irq_is_pending(const struct device *dev)
{
	struct uart_rockchip_data *data = dev->data;

	return !(data->iir & UART_IIR_NO_INT);
}

static int uart_rockchip_irq_update(const struct device *dev)
{
	struct uart_rockchip_data *data = dev->data;
	struct UART_REG *regs = uart_rockchip_regs(dev);

	data->iir = HAL_UART_GetIrqID(regs);
	if (data->iir == UART_IIR_BUSY) {
		/* Reading USR clears the busy-detect interrupt. */
		(void)regs->USR;
	}

	return 1;
}

static void uart_rockchip_irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb,
					 void *cb_data)
{
	struct uart_rockchip_data *data = dev->data;

	data->cb = cb;
	data->cb_data = cb_data;
}

static void uart_rockchip_isr(const struct device *dev)
{
	struct uart_rockchip_data *data = dev->data;

	if (data->cb != NULL) {
		data->cb(dev, data->cb_data);
	} else {
		uart_rockchip_irq_update(dev);
	}
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int uart_rockchip_init(const struct device *dev)
{
	const struct uart_rockchip_config *config = dev->config;
	struct uart_rockchip_data *data = dev->data;
	int ret;

	ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	HAL_CRU_ClkEnable(config->hal->pclkGateID);
	HAL_CRU_ClkEnable(config->hal->sclkGateID);

	ret = uart_rockchip_apply(dev, &data->uart_cfg);
	if (ret < 0) {
		return ret;
	}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	if (config->irq_config_func != NULL) {
		config->irq_config_func(dev);
	}
#endif

	return 0;
}

static DEVICE_API(uart, uart_rockchip_api) = {
	.poll_in = uart_rockchip_poll_in,
	.poll_out = uart_rockchip_poll_out,
	.err_check = uart_rockchip_err_check,
#ifdef CONFIG_UART_USE_RUNTIME_CONFIGURE
	.configure = uart_rockchip_configure,
	.config_get = uart_rockchip_config_get,
#endif
#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = uart_rockchip_fifo_fill,
	.fifo_read = uart_rockchip_fifo_read,
	.irq_tx_enable = uart_rockchip_irq_tx_enable,
	.irq_tx_disable = uart_rockchip_irq_tx_disable,
	.irq_tx_ready = uart_rockchip_irq_tx_ready,
	.irq_tx_complete = uart_rockchip_irq_tx_complete,
	.irq_rx_enable = uart_rockchip_irq_rx_enable,
	.irq_rx_disable = uart_rockchip_irq_rx_disable,
	.irq_rx_ready = uart_rockchip_irq_rx_ready,
	.irq_err_enable = uart_rockchip_irq_err_enable,
	.irq_err_disable = uart_rockchip_irq_err_disable,
	.irq_is_pending = uart_rockchip_irq_is_pending,
	.irq_update = uart_rockchip_irq_update,
	.irq_callback_set = uart_rockchip_irq_callback_set,
#endif
};

#define UART_ROCKCHIP_IS(n, idx) (DT_INST_REG_ADDR(n) == UART##idx##_BASE)

#define UART_ROCKCHIP_HAL_DEV(n)                                                                   \
	(UART_ROCKCHIP_IS(n, 0)   ? &g_uart0Dev                                                    \
	 : UART_ROCKCHIP_IS(n, 1) ? &g_uart1Dev                                                    \
	 : UART_ROCKCHIP_IS(n, 2) ? &g_uart2Dev                                                    \
	 : UART_ROCKCHIP_IS(n, 3) ? &g_uart3Dev                                                    \
	 : UART_ROCKCHIP_IS(n, 4) ? &g_uart4Dev                                                    \
				  : &g_uart5Dev)

#define UART_ROCKCHIP_IRQ_HANDLER(n)                                                               \
	static void uart_rockchip_irq_config_##n(const struct device *dev)                           \
	{                                                                                          \
		ARG_UNUSED(dev);                                                                   \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), uart_rockchip_isr,            \
			    DEVICE_DT_INST_GET(n), 0);                                             \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}

#define UART_ROCKCHIP_HAS_IRQ(n)                                                                   \
	UTIL_AND(IS_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN), DT_INST_IRQ_HAS_IDX(n, 0))

#define UART_ROCKCHIP_INIT(n)                                                                      \
	BUILD_ASSERT(UART_ROCKCHIP_IS(n, 0) || UART_ROCKCHIP_IS(n, 1) || UART_ROCKCHIP_IS(n, 2) || \
			     UART_ROCKCHIP_IS(n, 3) || UART_ROCKCHIP_IS(n, 4) ||                   \
			     UART_ROCKCHIP_IS(n, 5),                                               \
		     "rockchip,uart: unknown UART base address");                           \
                                                                                                   \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
                                                                                                   \
	IF_ENABLED(UART_ROCKCHIP_HAS_IRQ(n), (UART_ROCKCHIP_IRQ_HANDLER(n)))                                                                                 \
                                                                                                   \
	static const struct uart_rockchip_config uart_rockchip_config_##n = {                          \
		.hal = UART_ROCKCHIP_HAL_DEV(n),                                                   \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                         \
		IF_ENABLED(CONFIG_UART_INTERRUPT_DRIVEN,                       \
			   (.irq_config_func = COND_CODE_1(                    \
				    DT_INST_IRQ_HAS_IDX(n, 0),                 \
				    (uart_rockchip_irq_config_##n), (NULL)),)) };                \
                                                                                                   \
	static struct uart_rockchip_data uart_rockchip_data_##n = {                                    \
		.uart_cfg =                                                                        \
			{                                                                          \
				.baudrate = DT_INST_PROP(n, current_speed),                        \
				.parity = DT_INST_ENUM_IDX(n, parity),                             \
				.stop_bits =                                                       \
					DT_INST_ENUM_IDX_OR(n, stop_bits, UART_CFG_STOP_BITS_1),   \
				.data_bits =                                                       \
					DT_INST_ENUM_IDX_OR(n, data_bits, UART_CFG_DATA_BITS_8),   \
				.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,                              \
			},                                                                         \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, uart_rockchip_init, NULL, &uart_rockchip_data_##n,                    \
			      &uart_rockchip_config_##n, PRE_KERNEL_1, CONFIG_SERIAL_INIT_PRIORITY,  \
			      &uart_rockchip_api);

DT_INST_FOREACH_STATUS_OKAY(UART_ROCKCHIP_INIT)
