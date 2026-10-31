/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/irq_nextlevel.h>
#include <zephyr/platform/hooks.h>
#include <zephyr/sys/sys_io.h>

#define SGRF_PMU_CON3 0xFF96000CUL

void soc_early_init_hook(void)
{
	/* Single write: a stepwise clear of mcu_tcm_sel1/2 faults the running M0. */
	sys_write32(0x01F80000, SGRF_PMU_CON3);
}

#ifdef CONFIG_MULTI_LEVEL_INTERRUPTS

#define INTMUX_DEV DEVICE_DT_GET(DT_INST(0, rockchip_intmux))

void z_soc_irq_enable(unsigned int irq)
{
	if (irq_get_level(irq) == 1U) {
		arm_irq_enable(irq);
	} else {
		irq_enable_next_level(INTMUX_DEV, irq);
	}
}

void z_soc_irq_disable(unsigned int irq)
{
	if (irq_get_level(irq) == 1U) {
		arm_irq_disable(irq);
	} else {
		irq_disable_next_level(INTMUX_DEV, irq);
	}
}

int z_soc_irq_is_enabled(unsigned int irq)
{
	if (irq_get_level(irq) == 1U) {
		return arm_irq_is_enabled(irq);
	}

	return irq_line_is_enabled_next_level(INTMUX_DEV, irq);
}

/* Level-2 lines share the priority of their INTMUX group's NVIC line. */
void z_soc_irq_priority_set(unsigned int irq, unsigned int prio, unsigned int flags)
{
	if (irq_get_level(irq) == 1U) {
		arm_irq_priority_set(irq, prio, flags);
	}
}

#endif /* CONFIG_MULTI_LEVEL_INTERRUPTS */
