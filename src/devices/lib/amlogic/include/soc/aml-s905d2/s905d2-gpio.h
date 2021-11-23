// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_DEVICES_LIB_AMLOGIC_INCLUDE_SOC_AML_S905D2_S905D2_GPIO_H_
#define SRC_DEVICES_LIB_AMLOGIC_INCLUDE_SOC_AML_S905D2_S905D2_GPIO_H_

#define S905D2_GPIOZ_COUNT 16
#define S905D2_GPIOA_COUNT 16
#define S905D2_GPIOBOOT_COUNT 16
#define S905D2_GPIOC_COUNT 8
#define S905D2_GPIOX_COUNT 20
#define S905D2_GPIOH_COUNT 9
#define S905D2_GPIOAO_COUNT 12
#define S905D2_GPIOE_COUNT 3

#define S905D2_GPIOZ_START 0
#define S905D2_GPIOA_START S905D2_GPIOZ_COUNT
#define S905D2_GPIOBOOT_START (S905D2_GPIOA_START + S905D2_GPIOA_COUNT)
#define S905D2_GPIOC_START (S905D2_GPIOBOOT_START + S905D2_GPIOBOOT_COUNT)
#define S905D2_GPIOX_START (S905D2_GPIOC_START + S905D2_GPIOC_COUNT)
#define S905D2_GPIOH_START (S905D2_GPIOX_START + S905D2_GPIOX_COUNT)
#define S905D2_GPIOAO_START (S905D2_GPIOH_START + S905D2_GPIOH_COUNT)
#define S905D2_GPIOE_START (S905D2_GPIOAO_START + S905D2_GPIOAO_COUNT)

#define S905D2_GPIOZ(n) (S905D2_GPIOZ_START + n)
#define S905D2_GPIOA(n) (S905D2_GPIOA_START + n)
#define S905D2_GPIOBOOT(n) (S905D2_GPIOBOOT_START + n)
#define S905D2_GPIOC(n) (S905D2_GPIOC_START + n)
#define S905D2_GPIOX(n) (S905D2_GPIOX_START + n)
#define S905D2_GPIOH(n) (S905D2_GPIOH_START + n)
#define S905D2_GPIOAO(n) (S905D2_GPIOAO_START + n)
// GPIOE is contained in GPIO AO bank
#define S905D2_GPIOE(n) (S905D2_GPIOE_START + n)

#define S905D2_PREG_PAD_GPIO0_EN_N 0x10
#define S905D2_PREG_PAD_GPIO0_O 0x11
#define S905D2_PREG_PAD_GPIO0_I 0x12
#define S905D2_PREG_PAD_GPIO1_EN_N 0x13
#define S905D2_PREG_PAD_GPIO1_O 0x14
#define S905D2_PREG_PAD_GPIO1_I 0x15
#define S905D2_PREG_PAD_GPIO2_EN_N 0x16
#define S905D2_PREG_PAD_GPIO2_O 0x17
#define S905D2_PREG_PAD_GPIO2_I 0x18
#define S905D2_PREG_PAD_GPIO3_EN_N 0x19
#define S905D2_PREG_PAD_GPIO3_O 0x1a
#define S905D2_PREG_PAD_GPIO3_I 0x1b
#define S905D2_PREG_PAD_GPIO4_EN_N 0x1c
#define S905D2_PREG_PAD_GPIO4_O 0x1d
#define S905D2_PREG_PAD_GPIO4_I 0x1e
#define S905D2_PREG_PAD_GPIO5_EN_N 0x20
#define S905D2_PREG_PAD_GPIO5_O 0x21
#define S905D2_PREG_PAD_GPIO5_I 0x22

#define S905D2_PAD_PULL_UP_EN_REG0 0x48
#define S905D2_PAD_PULL_UP_EN_REG1 0x49
#define S905D2_PAD_PULL_UP_EN_REG2 0x4a
#define S905D2_PAD_PULL_UP_EN_REG3 0x4b
#define S905D2_PAD_PULL_UP_EN_REG4 0x4c
#define S905D2_PAD_PULL_UP_EN_REG5 0x4d

#define S905D2_PULL_UP_REG0 0x3a
#define S905D2_PULL_UP_REG1 0x3b
#define S905D2_PULL_UP_REG2 0x3c
#define S905D2_PULL_UP_REG3 0x3d
#define S905D2_PULL_UP_REG4 0x3e
#define S905D2_PULL_UP_REG5 0x3f

#define S905D2_AO_PAD_DS_A 0x07
#define S905D2_AO_PAD_DS_B 0x08
#define S905D2_PAD_DS_REG0A 0xd0
#define S905D2_PAD_DS_REG1A 0xd1
#define S905D2_PAD_DS_REG2A 0xd2
#define S905D2_PAD_DS_REG2B 0xd3
#define S905D2_PAD_DS_REG3A 0xd4
#define S905D2_PAD_DS_REG4A 0xd5
#define S905D2_PAD_DS_REG5A 0xd6

#define S905D2_PERIPHS_PIN_MUX_0 0xb0
#define S905D2_PERIPHS_PIN_MUX_1 0xb1
#define S905D2_PERIPHS_PIN_MUX_2 0xb2
#define S905D2_PERIPHS_PIN_MUX_3 0xb3
#define S905D2_PERIPHS_PIN_MUX_4 0xb4
#define S905D2_PERIPHS_PIN_MUX_5 0xb5
#define S905D2_PERIPHS_PIN_MUX_6 0xb6
#define S905D2_PERIPHS_PIN_MUX_7 0xb7
#define S905D2_PERIPHS_PIN_MUX_9 0xb9
#define S905D2_PERIPHS_PIN_MUX_B 0xbb
#define S905D2_PERIPHS_PIN_MUX_C 0xbc
#define S905D2_PERIPHS_PIN_MUX_D 0xbd
#define S905D2_PERIPHS_PIN_MUX_E 0xbe

#define S905D2_AO_GPIO_O_EN_N 0x09
#define S905D2_AO_GPIO_I 0x0a
#define S905D2_AO_GPIO_O 0x0d

#define S905D2_GPIOAO_PULL_UP_REG 0x0b
#define S905D2_GPIOAO_PULL_EN_REG 0x0c

#define S905D2_AO_RTI_PINMUX_REG0 0x05
#define S905D2_AO_RTI_PINMUX_REG1 0x06

// These are relative to base address 0xffd00000 and in sizeof(uint32_t)
#define S905D2_GPIO_INT_EDGE_POLARITY 0x3c20
#define S905D2_GPIO_0_3_PIN_SELECT 0x3c21
#define S905D2_GPIO_4_7_PIN_SELECT 0x3c22
#define S905D2_GPIO_FILTER_SELECT 0x3c23

#define S905D2_GPIOA0_PIN_START 0
#define S905D2_GPIOZ_PIN_START 12
#define S905D2_GPIOH_PIN_START 28
#define S905D2_GPIOBOOT_PIN_START 37
#define S905D2_GPIOC_PIN_START 53
#define S905D2_GPIOA_PIN_START 61
#define S905D2_GPIOX_PIN_START 77
#define S905D2_GPIOE_PIN_START 97

// GPIOA pin alternate functions
#define S905D2_GPIOA_1_TDMB_SCLK_FN 1
#define S905D2_GPIOA_1_TDMB_SLV_SCLK_FN 2
#define S905D2_GPIOA_2_TDMB_FS_FN 1
#define S905D2_GPIOA_2_TDMB_SLV_FS_FN 2
#define S905D2_GPIOA_3_TDMB_D0_FN 1
#define S905D2_GPIOA_3_TDMB_DIN0_FN 2
#define S905D2_GPIOA_6_PDM_DIN2_FN 1
#define S905D2_GPIOA_6_TDMB_DIN3_FN 2
#define S905D2_GPIOA_6_TDMB_D3_FN 3
#define S905D2_GPIOA_7_PDM_DCLK_FN 1
#define S905D2_GPIOA_7_TDMC_D3_FN 2
#define S905D2_GPIOA_7_TDMC_DIN3_FN 3
#define S905D2_GPIOA_8_PDM_DIN0_FN 1
#define S905D2_GPIOA_8_TDMC_D2_FN 2
#define S905D2_GPIOA_8_TDMC_DIN2_FN 3
#define S905D2_GPIOA_9_PDM_DIN1_FN 1
#define S905D2_GPIOA_9_TDMC_D1_FN 2
#define S905D2_GPIOA_9_TDMC_DIN1_FN 3

#define S905D2_GPIOX_8_TDMA_D1_FN 1
#define S905D2_GPIOX_8_TDMA_DIN1_FN 2
#define S905D2_GPIOX_9_TDMA_D0_FN 1
#define S905D2_GPIOX_9_TDMA_DIN0_FN 2
#define S905D2_GPIOX_10_TDMA_FS_FN 1
#define S905D2_GPIOX_11_TDMA_SCLK_FN 1

#define S905D2_GPIOZ_2_TDMC_D0_FN 4
#define S905D2_GPIOZ_3_TDMC_D1_FN 4
#define S905D2_GPIOZ_4_TDMC_D2_FN 4
#define S905D2_GPIOZ_5_TDMC_D3_FN 4
#define S905D2_GPIOZ_6_TDMC_FS_FN 4
#define S905D2_GPIOZ_7_TDMC_SCLK_FN 4

#define S905D2_GPIOAO_9_MCLK_FN 5

#endif  // SRC_DEVICES_LIB_AMLOGIC_INCLUDE_SOC_AML_S905D2_S905D2_GPIO_H_
