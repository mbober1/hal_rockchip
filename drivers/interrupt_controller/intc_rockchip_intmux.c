/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT rockchip_intmux

#include "hal_base.h"
#include "hal_cru.h"

#include <zephyr/device.h>
#include <zephyr/devicetree/interrupt_controller.h>
#include <zephyr/irq.h>
#include <zephyr/irq_multilevel.h>
#include <zephyr/irq_nextlevel.h>
#include <zephyr/sw_isr_table.h>

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1, "Only one INTMUX instance exists");
BUILD_ASSERT(CONFIG_MAX_IRQ_PER_AGGREGATOR == INTMUX_NUM_INT_PER_GROUP);
BUILD_ASSERT(CONFIG_2ND_LVL_INTR_00_OFFSET == INTMUX_OUT0_IRQn &&
	     CONFIG_2ND_LVL_INTR_01_OFFSET == INTMUX_OUT1_IRQn &&
	     CONFIG_2ND_LVL_INTR_02_OFFSET == INTMUX_OUT2_IRQn &&
	     CONFIG_2ND_LVL_INTR_03_OFFSET == INTMUX_OUT3_IRQn,
	     "2ND_LVL_INTR_0n_OFFSET must match the INTMUX NVIC lines");

#define INTMUX_REGS ((struct INTMUX_REG *)DT_INST_REG_ADDR(0))

static inline uint32_t intmux_group(unsigned int irq)
{
	return irq_parent_level_2(irq) - INTMUX_OUT_IRQ_START_NUM;
}

static void intmux_rockchip_irq_enable(const struct device *dev, unsigned int irq)
{
	unsigned int key = irq_lock();

	ARG_UNUSED(dev);
	INTMUX_REGS->INT_ENABLE_GROUP[intmux_group(irq)] |= BIT(irq_from_level_2(irq));
	irq_unlock(key);
}

static void intmux_rockchip_irq_disable(const struct device *dev, unsigned int irq)
{
	unsigned int key = irq_lock();

	ARG_UNUSED(dev);
	INTMUX_REGS->INT_ENABLE_GROUP[intmux_group(irq)] &= ~BIT(irq_from_level_2(irq));
	irq_unlock(key);
}

static unsigned int intmux_rockchip_get_state(const struct device *dev)
{
	ARG_UNUSED(dev);

	for (int i = 0; i < INTMUX_NUM_GROUP_PER_CON; i++) {
		if (INTMUX_REGS->INT_ENABLE_GROUP[i] != 0U) {
			return 1;
		}
	}

	return 0;
}

static int intmux_rockchip_get_line_state(const struct device *dev, unsigned int irq)
{
	ARG_UNUSED(dev);

	return (INTMUX_REGS->INT_ENABLE_GROUP[intmux_group(irq)] &
		BIT(irq_from_level_2(irq))) != 0U;
}

/* FLAG is the masked source level, so each source ISR must clear its own interrupt. */
static void intmux_rockchip_isr(const void *arg)
{
	uint32_t group = POINTER_TO_UINT(arg);
	uint32_t flags = INTMUX_REGS->INT_FLAG_GROUP[group];
	const struct _isr_table_entry *base =
		&_sw_isr_table[CONFIG_2ND_LVL_ISR_TBL_OFFSET + group * CONFIG_MAX_IRQ_PER_AGGREGATOR];

	while (flags != 0U) {
		uint32_t line = find_lsb_set(flags) - 1U;

		base[line].isr(base[line].arg);
		flags &= ~BIT(line);
	}
}

static const struct irq_next_level_api intmux_rockchip_api = {
	.intr_enable = intmux_rockchip_irq_enable,
	.intr_disable = intmux_rockchip_irq_disable,
	.intr_get_state = intmux_rockchip_get_state,
	.intr_get_line_state = intmux_rockchip_get_line_state,
};

#define INTMUX_CH_CONNECT(node_id)                                                                 \
	IRQ_CONNECT(DT_IRQN(node_id), DT_IRQ(node_id, priority), intmux_rockchip_isr,               \
		    UINT_TO_POINTER(DT_IRQN(node_id) - INTMUX_OUT_IRQ_START_NUM), 0);              \
	irq_enable(DT_IRQN(node_id));

static int intmux_rockchip_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	HAL_CRU_ClkEnable(PCLK_INTMUX_GATE);

	for (int i = 0; i < INTMUX_NUM_GROUP_PER_CON; i++) {
		INTMUX_REGS->INT_ENABLE_GROUP[i] = 0U;
	}

	DT_INST_FOREACH_CHILD_STATUS_OKAY(0, INTMUX_CH_CONNECT)

	return 0;
}

DEVICE_DT_INST_DEFINE(0, intmux_rockchip_init, NULL, NULL, NULL, PRE_KERNEL_1,
		      CONFIG_INTC_INIT_PRIORITY, &intmux_rockchip_api);

#define INTMUX_CH_PARENT_ENTRY(node_id)                                                            \
	IRQ_PARENT_ENTRY_DEFINE(CONCAT(intmux_rockchip_ch, DT_NODE_CHILD_IDX(node_id)),              \
				DEVICE_DT_INST_GET(0), DT_IRQN(node_id),                           \
				INTC_CHILD_ISR_TBL_OFFSET(node_id),                                \
				DT_INTC_GET_AGGREGATOR_LEVEL(node_id));

DT_INST_FOREACH_CHILD_STATUS_OKAY(0, INTMUX_CH_PARENT_ENTRY)
