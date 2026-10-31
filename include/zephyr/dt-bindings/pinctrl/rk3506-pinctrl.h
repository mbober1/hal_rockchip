/*
 * Copyright (c) 2026 Marcin Bober
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_PINCTRL_RK3506_PINCTRL_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_PINCTRL_RK3506_PINCTRL_H_

/** Position of the pin field. */
#define RK3506_PIN_POS   0U
/** Mask for the pin field. */
#define RK3506_PIN_MASK  0x1FU
/** Position of the bank field. */
#define RK3506_BANK_POS  5U
/** Mask for the bank field. */
#define RK3506_BANK_MASK 0x7U
/** Position of the function field. */
#define RK3506_FUNC_POS  8U
/** Mask for the function field. */
#define RK3506_FUNC_MASK 0xFU
/** Position of the RMIO field. */
#define RK3506_RMIO_POS  16U
/** Mask for the RMIO field. */
#define RK3506_RMIO_MASK 0xFFU
#define RK3506_RMIO_FLAG (1U << 31)

/* Plain IOMUX function select (0 = GPIO). */
#define RK3506_PINMUX(bank, pin, func)                                         \
	((((bank) & RK3506_BANK_MASK) << RK3506_BANK_POS) |                    \
	 (((pin) & RK3506_PIN_MASK) << RK3506_PIN_POS) |                       \
	 (((func) & RK3506_FUNC_MASK) << RK3506_FUNC_POS))

/* Route an RMIO matrix function to a pin (only some pins on banks 0 and 1). */
#define RK3506_RMIO(bank, pin, rmio)                                           \
	(RK3506_RMIO_FLAG |                                                    \
	 (((bank) & RK3506_BANK_MASK) << RK3506_BANK_POS) |                    \
	 (((pin) & RK3506_PIN_MASK) << RK3506_PIN_POS) |                       \
	 (((rmio) & RK3506_RMIO_MASK) << RK3506_RMIO_POS))

#define RK_PA0 0
#define RK_PA1 1
#define RK_PA2 2
#define RK_PA3 3
#define RK_PA4 4
#define RK_PA5 5
#define RK_PA6 6
#define RK_PA7 7
#define RK_PB0 8
#define RK_PB1 9
#define RK_PB2 10
#define RK_PB3 11
#define RK_PB4 12
#define RK_PB5 13
#define RK_PB6 14
#define RK_PB7 15
#define RK_PC0 16
#define RK_PC1 17
#define RK_PC2 18
#define RK_PC3 19
#define RK_PC4 20
#define RK_PC5 21
#define RK_PC6 22
#define RK_PC7 23
#define RK_PD0 24
#define RK_PD1 25
#define RK_PD2 26
#define RK_PD3 27
#define RK_PD4 28
#define RK_PD5 29
#define RK_PD6 30
#define RK_PD7 31

/* RMIO function IDs, values match eRMIO_Name in the HAL soc.h. */
#define RK3506_RMIO_UART1_TX      0x01
#define RK3506_RMIO_UART1_RX      0x02
#define RK3506_RMIO_UART2_TX      0x03
#define RK3506_RMIO_UART2_RX      0x04
#define RK3506_RMIO_UART3_TX      0x05
#define RK3506_RMIO_UART3_RX      0x06
#define RK3506_RMIO_UART3_CTS_REN 0x07
#define RK3506_RMIO_UART3_RTSN_DE 0x08
#define RK3506_RMIO_UART4_TX      0x09
#define RK3506_RMIO_UART4_RX      0x0A
#define RK3506_RMIO_UART4_CTS_REN 0x0B
#define RK3506_RMIO_UART4_RTSN_DE 0x0C
#define RK3506_RMIO_PWM0_CH0      0x1E
#define RK3506_RMIO_PWM0_CH1      0x1F
#define RK3506_RMIO_PWM0_CH2      0x20
#define RK3506_RMIO_PWM0_CH3      0x21
#define RK3506_RMIO_PWM1_CH0      0x22
#define RK3506_RMIO_PWM1_CH1      0x23
#define RK3506_RMIO_PWM1_CH2      0x24
#define RK3506_RMIO_PWM1_CH3      0x25
#define RK3506_RMIO_PWM1_CH4      0x26
#define RK3506_RMIO_PWM1_CH5      0x27
#define RK3506_RMIO_PWM1_CH6      0x28
#define RK3506_RMIO_PWM1_CH7      0x29

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_PINCTRL_RK3506_PINCTRL_H_ */
