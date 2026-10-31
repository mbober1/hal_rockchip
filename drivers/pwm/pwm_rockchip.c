/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rockchip_pwm

#include "hal_base.h"
#include "hal_cru.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/irq.h>
#include <zephyr/sys/util.h>

#define PWM_CLK_RATE    PLL_INPUT_OSC_RATE
#define PWM_CLK_SRC_OSC 1U

/* All PWM v4 control registers use a 16-bit write-enable mask in the upper half. */
#define HIWORD(mask, val) (((uint32_t)(mask) << 16) | (val))

#define PWM_CAP_INTS (PWM_INTSTS_CAP_LPC_INTSTS_MASK | PWM_INTSTS_CAP_HPC_INTSTS_MASK)

struct pwm_rockchip_config {
	struct PWM_REG *reg;
	const struct pinctrl_dev_config *pcfg;
	uint32_t pclk_gate;
	uint32_t clk_gate;
	uint32_t osc_gate;
	uint8_t num_channels;
#ifdef CONFIG_PWM_CAPTURE
	uint8_t irq_mask;
	unsigned int irqs[PWM_CHANNEL_MAX];
#endif
};

#ifdef CONFIG_PWM_CAPTURE
struct pwm_rockchip_capture {
	pwm_capture_callback_handler_t cb;
	void *user_data;
	uint8_t seen;
	uint8_t skip;
	bool continuous;
	bool inverted;
	bool enabled;
};

struct pwm_rockchip_irq_arg {
	const struct device *dev;
	uint8_t channel;
};
#endif

struct pwm_rockchip_data {
	struct PWM_HANDLE hal;
#ifdef CONFIG_PWM_CAPTURE
	struct pwm_rockchip_capture cap[PWM_CHANNEL_MAX];
#endif
};

static inline struct PWM_REG *pwm_rockchip_ch_reg(const struct device *dev, uint32_t channel)
{
	struct pwm_rockchip_data *data = dev->data;

	return data->hal.pChHandle[channel].pReg;
}

static int pwm_rockchip_set_cycles(const struct device *dev, uint32_t channel,
				   uint32_t period_cycles, uint32_t pulse_cycles, pwm_flags_t flags)
{
	const struct pwm_rockchip_config *cfg = dev->config;
	struct pwm_rockchip_data *data = dev->data;
	struct PWM_REG *reg;
	uint32_t pol;

	if (channel >= cfg->num_channels || pulse_cycles > period_cycles) {
		return -EINVAL;
	}
#ifdef CONFIG_PWM_CAPTURE
	if (data->cap[channel].enabled) {
		return -EBUSY;
	}
#endif

	reg = pwm_rockchip_ch_reg(dev, channel);
	pol = (flags & PWM_POLARITY_INVERTED) ? PWM_CTRL_INACTIVE_POL_MASK : PWM_CTRL_DUTY_POL_MASK;

	if (period_cycles == 0U) {
		WRITE_REG(reg->CTRL,
			  HIWORD(PWM_CTRL_DUTY_POL_MASK | PWM_CTRL_INACTIVE_POL_MASK, pol));
		HAL_PWM_Disable(&data->hal, channel);
		return 0;
	}

	WRITE_REG(reg->PERIOD, period_cycles);
	WRITE_REG(reg->DUTY, pulse_cycles);
	WRITE_REG(reg->CTRL, HIWORD(PWM_CTRL_DUTY_POL_MASK | PWM_CTRL_INACTIVE_POL_MASK |
					    PWM_CTRL_OUTPUT_MODE_MASK | PWM_CTRL_PWM_IN_SEL_MASK,
				    pol));
	WRITE_REG(reg->ENABLE,
		  HIWORD(PWM_ENABLE_PWM_CTRL_UPDATE_EN_MASK, PWM_ENABLE_PWM_CTRL_UPDATE_EN_MASK));
	HAL_PWM_Enable(&data->hal, channel, HAL_PWM_CONTINUOUS);

	return 0;
}

static int pwm_rockchip_get_cycles_per_sec(const struct device *dev, uint32_t channel,
					   uint64_t *cycles)
{
	const struct pwm_rockchip_config *cfg = dev->config;

	if (channel >= cfg->num_channels) {
		return -EINVAL;
	}

	*cycles = PWM_CLK_RATE;

	return 0;
}

#ifdef CONFIG_PWM_CAPTURE
static void pwm_rockchip_capture_stop(const struct device *dev, uint32_t channel)
{
	const struct pwm_rockchip_config *cfg = dev->config;
	struct pwm_rockchip_data *data = dev->data;
	struct PWM_REG *reg = pwm_rockchip_ch_reg(dev, channel);

	irq_disable(cfg->irqs[channel]);
	WRITE_REG(reg->INT_EN, HIWORD(PWM_CAP_INTS, 0));
	HAL_PWM_Disable(&data->hal, channel);
	WRITE_REG(reg->INTSTS, PWM_CAP_INTS);
	data->cap[channel].enabled = false;
}

static void pwm_rockchip_isr(const void *arg)
{
	const struct pwm_rockchip_irq_arg *a = arg;
	struct pwm_rockchip_data *data = a->dev->data;
	struct pwm_rockchip_capture *cap = &data->cap[a->channel];
	struct PWM_REG *reg = pwm_rockchip_ch_reg(a->dev, a->channel);
	uint32_t status = READ_REG(reg->INTSTS) & PWM_CAP_INTS;
	uint32_t hpc, lpc;

	WRITE_REG(reg->INTSTS, status);

	/* The first level after enable may be partial, so drop one of each. */
	cap->seen |= status & ~cap->skip;
	cap->skip &= ~status;

	if (!cap->enabled || cap->seen != PWM_CAP_INTS) {
		return;
	}

	cap->seen = 0U;
	hpc = READ_REG(reg->HPC);
	lpc = READ_REG(reg->LPC);

	if (!cap->continuous) {
		pwm_rockchip_capture_stop(a->dev, a->channel);
	}

	cap->cb(a->dev, a->channel, hpc + lpc, cap->inverted ? lpc : hpc, 0, cap->user_data);
}

static int pwm_rockchip_configure_capture(const struct device *dev, uint32_t channel,
					  pwm_flags_t flags, pwm_capture_callback_handler_t cb,
					  void *user_data)
{
	const struct pwm_rockchip_config *cfg = dev->config;
	struct pwm_rockchip_data *data = dev->data;
	struct pwm_rockchip_capture *cap;

	if (channel >= cfg->num_channels) {
		return -EINVAL;
	}
	if ((cfg->irq_mask & BIT(channel)) == 0U) {
		return -ENOTSUP;
	}

	cap = &data->cap[channel];
	if (cap->enabled) {
		return -EBUSY;
	}

	cap->cb = cb;
	cap->user_data = user_data;
	cap->continuous = (flags & PWM_CAPTURE_MODE_MASK) == PWM_CAPTURE_MODE_CONTINUOUS;
	cap->inverted = (flags & PWM_POLARITY_MASK) == PWM_POLARITY_INVERTED;

	return 0;
}

static int pwm_rockchip_enable_capture(const struct device *dev, uint32_t channel)
{
	const struct pwm_rockchip_config *cfg = dev->config;
	struct pwm_rockchip_data *data = dev->data;
	struct pwm_rockchip_capture *cap;
	struct PWM_REG *reg;

	if (channel >= cfg->num_channels) {
		return -EINVAL;
	}

	cap = &data->cap[channel];
	if (cap->cb == NULL) {
		return -EINVAL;
	}
	if (cap->enabled) {
		return -EBUSY;
	}

	reg = pwm_rockchip_ch_reg(dev, channel);
	HAL_PWM_Disable(&data->hal, channel);
	WRITE_REG(reg->INTSTS, PWM_CAP_INTS);
	WRITE_REG(reg->CTRL,
		  HIWORD(PWM_CTRL_PWM_IN_SEL_MASK, channel << PWM_CTRL_PWM_IN_SEL_SHIFT));

	cap->seen = 0U;
	cap->skip = PWM_CAP_INTS;
	cap->enabled = true;

	/* HAL_PWM_EnableCaptureInt() also writes PWM_IN_SEL into INT_EN, so set INT_EN here. */
	WRITE_REG(reg->INT_EN, HIWORD(PWM_CAP_INTS, PWM_CAP_INTS));
	irq_enable(cfg->irqs[channel]);
	HAL_PWM_Enable(&data->hal, channel, HAL_PWM_CAPTURE);

	return 0;
}

static int pwm_rockchip_disable_capture(const struct device *dev, uint32_t channel)
{
	const struct pwm_rockchip_config *cfg = dev->config;
	struct pwm_rockchip_data *data = dev->data;
	unsigned int key;

	if (channel >= cfg->num_channels) {
		return -EINVAL;
	}

	key = irq_lock();
	if (data->cap[channel].enabled) {
		pwm_rockchip_capture_stop(dev, channel);
	}
	irq_unlock(key);

	return 0;
}
#endif /* CONFIG_PWM_CAPTURE */

static DEVICE_API(pwm, pwm_rockchip_api) = {
	.set_cycles = pwm_rockchip_set_cycles,
	.get_cycles_per_sec = pwm_rockchip_get_cycles_per_sec,
#ifdef CONFIG_PWM_CAPTURE
	.configure_capture = pwm_rockchip_configure_capture,
	.enable_capture = pwm_rockchip_enable_capture,
	.disable_capture = pwm_rockchip_disable_capture,
#endif
};

static int pwm_rockchip_init(const struct device *dev)
{
	const struct pwm_rockchip_config *cfg = dev->config;
	struct pwm_rockchip_data *data = dev->data;
	int ret;

	HAL_CRU_ClkEnable(cfg->pclk_gate);
	HAL_CRU_ClkEnable(cfg->clk_gate);
	HAL_CRU_ClkEnable(cfg->osc_gate);

	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0 && ret != -ENOENT) {
		return ret;
	}

	HAL_PWM_Init(&data->hal, cfg->reg, PWM_CLK_RATE);
	if (data->hal.channelNum < cfg->num_channels) {
		return -EIO;
	}

	/* Channels may still run from previously loaded firmware. */
	for (uint32_t ch = 0; ch < cfg->num_channels; ch++) {
		struct PWM_REG *reg = pwm_rockchip_ch_reg(dev, ch);

		HAL_PWM_Disable(&data->hal, ch);
		WRITE_REG(reg->INT_EN, HIWORD(PWM_CAP_INTS, 0));
		WRITE_REG(reg->INTSTS, PWM_CAP_INTS);
		WRITE_REG(reg->CLK_CTRL,
			  HIWORD(PWM_CLK_CTRL_PRESCALE_MASK | PWM_CLK_CTRL_SCALE_MASK |
					 PWM_CLK_CTRL_CLK_SRC_SEL_MASK |
					 PWM_CLK_CTRL_CLK_GLOBAL_SEL_MASK,
				 PWM_CLK_SRC_OSC << PWM_CLK_CTRL_CLK_SRC_SEL_SHIFT));
	}

	return 0;
}

#define PWM_IS_PWM0(n) (DT_INST_REG_ADDR(n) == PWM0_BASE)

#ifdef CONFIG_PWM_CAPTURE
/* INTMUX channels can only be wired when the INTMUX driver is enabled. */
#define PWM_CH_IRQ_OK(child)                                                                       \
	UTIL_OR(IS_EQ(DT_IRQ_LEVEL(child), 1), IS_ENABLED(CONFIG_INTC_ROCKCHIP_INTMUX))

#define PWM_CH_IRQ_ARG(child, n)                                                                   \
	[DT_REG_ADDR(child)] = {.dev = DEVICE_DT_INST_GET(n), .channel = DT_REG_ADDR(child)},

#define PWM_CH_IRQ_CONNECT(child, n)                                                               \
	COND_CODE_1(PWM_CH_IRQ_OK(child),                                                          \
		    (IRQ_CONNECT(DT_IRQN(child),                                                   \
				 COND_CODE_1(DT_IRQ_HAS_CELL(child, priority),                     \
					     (DT_IRQ(child, priority)), (0)),                      \
				 pwm_rockchip_isr, &pwm_rockchip_irq_args_##n[DT_REG_ADDR(child)], 0);),   \
		    ())

#define PWM_CH_IRQN(child)                                                                         \
	COND_CODE_1(PWM_CH_IRQ_OK(child), ([DT_REG_ADDR(child)] = DT_IRQN(child),), ())

#define PWM_CH_IRQ_BIT(child) | COND_CODE_1(PWM_CH_IRQ_OK(child), (BIT(DT_REG_ADDR(child))), (0))

#define PWM_ROCKCHIP_IRQ_DEFINE(n)                                                                 \
	static const struct pwm_rockchip_irq_arg __maybe_unused                                    \
		pwm_rockchip_irq_args_##n[PWM_CHANNEL_MAX] = {                                     \
			DT_INST_FOREACH_CHILD_VARGS(n, PWM_CH_IRQ_ARG, n)};                        \
	static int pwm_rockchip_irq_init_##n(void)                                                 \
	{                                                                                          \
		DT_INST_FOREACH_CHILD_VARGS(n, PWM_CH_IRQ_CONNECT, n)                              \
		return 0;                                                                          \
	}                                                                                          \
	SYS_INIT(pwm_rockchip_irq_init_##n, PRE_KERNEL_1, 0);

#define PWM_ROCKCHIP_IRQ_CFG(n)                                                                    \
	.irq_mask = 0 DT_INST_FOREACH_CHILD(n, PWM_CH_IRQ_BIT),                                    \
	.irqs = {DT_INST_FOREACH_CHILD(n, PWM_CH_IRQN)},
#else
#define PWM_ROCKCHIP_IRQ_DEFINE(n)
#define PWM_ROCKCHIP_IRQ_CFG(n)
#endif

#define PWM_CH_CHECK(child, n)                                                                     \
	BUILD_ASSERT(DT_REG_ADDR(child) < DT_INST_REG_SIZE(n) / PWM_CHANNEL_OFFSET,                \
		     "PWM channel index out of range");

#define PWM_ROCKCHIP_INIT(n)                                                                       \
	BUILD_ASSERT(DT_INST_REG_ADDR(n) == PWM0_BASE || DT_INST_REG_ADDR(n) == PWM1_BASE,         \
		     "Unknown PWM base");                                                          \
	DT_INST_FOREACH_CHILD_VARGS(n, PWM_CH_CHECK, n)                                            \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	PWM_ROCKCHIP_IRQ_DEFINE(n)                                                                 \
	static struct pwm_rockchip_data pwm_rockchip_data_##n;                                     \
	static const struct pwm_rockchip_config pwm_rockchip_cfg_##n = {                           \
		.reg = (struct PWM_REG *)DT_INST_REG_ADDR(n),                                      \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                         \
		.pclk_gate = PWM_IS_PWM0(n) ? PCLK_PWM0_GATE : PCLK_PWM1_GATE,                     \
		.clk_gate = PWM_IS_PWM0(n) ? CLK_PWM0_GATE : CLK_PWM1_GATE,                        \
		.osc_gate = PWM_IS_PWM0(n) ? CLK_OSC_PWM0_GATE : CLK_OSC_PWM1_GATE,                \
		.num_channels = DT_INST_REG_SIZE(n) / PWM_CHANNEL_OFFSET,                          \
		PWM_ROCKCHIP_IRQ_CFG(n)};                                                          \
	DEVICE_DT_INST_DEFINE(n, pwm_rockchip_init, NULL, &pwm_rockchip_data_##n,                  \
			      &pwm_rockchip_cfg_##n, POST_KERNEL, CONFIG_PWM_INIT_PRIORITY,        \
			      &pwm_rockchip_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_ROCKCHIP_INIT)
