/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hal_pinctrl.h"

#include <errno.h>

#include <zephyr/drivers/pinctrl.h>
#include <zephyr/sys/util.h>

static const ePINCTRL_configParam rockchip_pull[] = {
	[ROCKCHIP_PULL_DISABLE] = PIN_CONFIG_PUL_NORMAL,
	[ROCKCHIP_PULL_UP] = PIN_CONFIG_PUL_UP,
	[ROCKCHIP_PULL_DOWN] = PIN_CONFIG_PUL_DOWN,
};

static const ePINCTRL_configParam rockchip_drive[] = {
	PIN_CONFIG_DRV_LEVEL0, PIN_CONFIG_DRV_LEVEL1, PIN_CONFIG_DRV_LEVEL2,
	PIN_CONFIG_DRV_LEVEL3, PIN_CONFIG_DRV_LEVEL4, PIN_CONFIG_DRV_LEVEL5,
};

static int pinctrl_rockchip_configure_pin(const pinctrl_soc_pin_t *pin)
{
	eGPIO_bankId bank = (pin->pinmux >> RK_BANK_POS) & RK_BANK_MASK;
	uint32_t mask = BIT((pin->pinmux >> RK_PIN_POS) & RK_PIN_MASK);
	uint32_t param = 0;
	HAL_Status rc;

	if (pin->pinmux & RK_RMIO_FLAG) {
		eRMIO_Name rmio = (pin->pinmux >> RK_RMIO_POS) & RK_RMIO_MASK;

		rc = HAL_PINCTRL_SetRMIO(bank, mask, rmio);
	} else {
		uint32_t func = (pin->pinmux >> RK_FUNC_POS) & RK_FUNC_MASK;

		rc = HAL_PINCTRL_SetIOMUX(bank, mask, PIN_CONFIG_MUX_FUNC0 | (func << SHIFT_MUX));
	}

	if (rc != HAL_OK) {
		return -EINVAL;
	}

	if (pin->pull != ROCKCHIP_PULL_KEEP) {
		param |= rockchip_pull[pin->pull];
	}

	if (pin->drive != 0U) {
		param |= rockchip_drive[pin->drive - 1U];
	}

	if (pin->schmitt) {
		param |= PIN_CONFIG_SMT_ENABLE;
	}

	if (param != 0U && HAL_PINCTRL_SetParam(bank, mask, param) != HAL_OK) {
		return -EINVAL;
	}

	return 0;
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg)
{
	ARG_UNUSED(reg);

	for (uint8_t i = 0U; i < pin_cnt; i++) {
		int ret = pinctrl_rockchip_configure_pin(&pins[i]);

		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}
