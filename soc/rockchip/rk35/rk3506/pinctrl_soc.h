/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_ROCKCHIP_ROCKCHIP_PINCTRL_SOC_H_
#define ZEPHYR_SOC_ROCKCHIP_ROCKCHIP_PINCTRL_SOC_H_

#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/pinctrl/rockchip-pinctrl.h>

#define ROCKCHIP_PULL_KEEP    0
#define ROCKCHIP_PULL_DISABLE 1
#define ROCKCHIP_PULL_UP      2
#define ROCKCHIP_PULL_DOWN    3

typedef struct pinctrl_soc_pin {
	uint32_t pinmux;
	uint32_t pull: 2;
	/* 0 leaves drive unchanged, otherwise level + 1 */
	uint32_t drive: 3;
	uint32_t schmitt: 1;
} pinctrl_soc_pin_t;

#define Z_PINCTRL_ROCKCHIP_PULL(node_id)                                                           \
	(DT_PROP(node_id, bias_pull_up)     ? ROCKCHIP_PULL_UP                                     \
	 : DT_PROP(node_id, bias_pull_down) ? ROCKCHIP_PULL_DOWN                                   \
	 : DT_PROP(node_id, bias_disable)   ? ROCKCHIP_PULL_DISABLE                                \
					    : ROCKCHIP_PULL_KEEP)

#define Z_PINCTRL_STATE_PIN_INIT(node_id, prop, idx)                                               \
	{                                                                                          \
		.pinmux = DT_PROP_BY_IDX(node_id, prop, idx),                                      \
		.pull = Z_PINCTRL_ROCKCHIP_PULL(node_id),                                          \
		.drive = DT_PROP_OR(node_id, drive_strength, -1) + 1,                              \
		.schmitt = DT_PROP(node_id, input_schmitt_enable),                                 \
	},

#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop)                                                   \
	{DT_FOREACH_CHILD_VARGS(DT_PHANDLE(node_id, prop), DT_FOREACH_PROP_ELEM, pinmux,           \
				Z_PINCTRL_STATE_PIN_INIT)}

#endif /* ZEPHYR_SOC_ROCKCHIP_ROCKCHIP_PINCTRL_SOC_H_ */
