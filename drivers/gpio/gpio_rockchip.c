/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rockchip_gpio

#include "hal_gpio.h"
#include "hal_pinctrl.h"

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/sys/util.h>

struct gpio_rockchip_config {
	struct gpio_driver_config common;
	struct GPIO_REG *regs;
	eGPIO_bankId bank;
};

static int gpio_rockchip_pin_configure(const struct device *dev, gpio_pin_t pin, gpio_flags_t flags)
{
	const struct gpio_rockchip_config *cfg = dev->config;
	ePINCTRL_GPIO_PINS mask = (ePINCTRL_GPIO_PINS)BIT(pin);

	if (flags & (GPIO_SINGLE_ENDED | GPIO_LINE_OPEN_DRAIN | GPIO_PULL_UP | GPIO_PULL_DOWN)) {
		return -ENOTSUP;
	}

	if (flags & GPIO_DISCONNECTED) {
		return 0;
	}

	HAL_PINCTRL_SetIOMUX(cfg->bank, mask, PIN_CONFIG_MUX_FUNC0);

	if (flags & GPIO_OUTPUT) {
		if (flags & GPIO_OUTPUT_INIT_HIGH) {
			HAL_GPIO_SetPinLevel(cfg->regs, mask, GPIO_HIGH);
		} else if (flags & GPIO_OUTPUT_INIT_LOW) {
			HAL_GPIO_SetPinLevel(cfg->regs, mask, GPIO_LOW);
		}
		HAL_GPIO_SetPinDirection(cfg->regs, mask, GPIO_OUT);
	} else if (flags & GPIO_INPUT) {
		HAL_GPIO_SetPinDirection(cfg->regs, mask, GPIO_IN);
	}

	return 0;
}

static int gpio_rockchip_port_set_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_rockchip_config *cfg = dev->config;

	HAL_GPIO_SetPinLevel(cfg->regs, (ePINCTRL_GPIO_PINS)pins, GPIO_HIGH);

	return 0;
}

static int gpio_rockchip_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_rockchip_config *cfg = dev->config;

	HAL_GPIO_SetPinLevel(cfg->regs, (ePINCTRL_GPIO_PINS)pins, GPIO_LOW);

	return 0;
}

static int gpio_rockchip_port_toggle_bits(const struct device *dev, gpio_port_pins_t pins)
{
	const struct gpio_rockchip_config *cfg = dev->config;

	for (unsigned int i = 0; i < 32; i++) {
		ePINCTRL_GPIO_PINS mask = (ePINCTRL_GPIO_PINS)BIT(i);

		if (!(pins & BIT(i))) {
			continue;
		}

		if (HAL_GPIO_GetPinLevel(cfg->regs, mask) == GPIO_HIGH) {
			HAL_GPIO_SetPinLevel(cfg->regs, mask, GPIO_LOW);
		} else {
			HAL_GPIO_SetPinLevel(cfg->regs, mask, GPIO_HIGH);
		}
	}

	return 0;
}

static const struct gpio_driver_api gpio_rockchip_api = {
	.pin_configure = gpio_rockchip_pin_configure,
	.port_set_bits_raw = gpio_rockchip_port_set_bits_raw,
	.port_clear_bits_raw = gpio_rockchip_port_clear_bits_raw,
	.port_toggle_bits = gpio_rockchip_port_toggle_bits,
};

#define GPIO_ROCKCHIP_INIT(n)                                                                      \
	static const struct gpio_rockchip_config gpio_rockchip_cfg_##n = {                         \
		.common =                                                                          \
			{                                                                          \
				.port_pin_mask = GPIO_PORT_PIN_MASK_FROM_DT_INST(n),               \
			},                                                                         \
		.regs = (struct GPIO_REG *)DT_INST_REG_ADDR(n),                                    \
		.bank = (eGPIO_bankId)DT_INST_PROP(n, bank_id),                                    \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &gpio_rockchip_cfg_##n, POST_KERNEL,            \
			      CONFIG_GPIO_INIT_PRIORITY, &gpio_rockchip_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_ROCKCHIP_INIT)
