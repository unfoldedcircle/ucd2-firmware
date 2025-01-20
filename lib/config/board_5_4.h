// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Revision 5.4
// Dock with Ethernet and controllable ETH status LED
//

#pragma once

#include <ETH.h>

#define HAS_ETHERNET

#define CHARGE_SENSE_GPIO    35
#define CHARGE_ENABLE_GPIO   12
#define BUTTON_GPIO          39

#define IR_RECEIVE_PIN       36
#define IR_SEND_PIN_INT_SIDE 5
#define IR_SEND_PIN_INT_TOP  13
#define IR_SEND_PIN_EXT_1    15
#define IR_SEND_PIN_EXT_2    2

#define STATUS_LED_R_PIN     32
#define STATUS_LED_G_PIN     33
#define STATUS_LED_B_PIN     14

#define ETH_STATUS_LED       1
#define ETH_CLK_EN           4
// #define ETH_CLK_INPUT        0
#define ETH_CLK_MODE         ETH_CLOCK_GPIO0_IN
#define ETH_POWER_PIN        -1
#define ETH_TYPE             ETH_PHY_LAN8720
#define ETH_ADDR             1
#define ETH_MDC_PIN          23
#define ETH_MDIO_PIN         18

#define ETH_TXD0_PIN         18
#define ETH_TXD1_PIN         22
#define ETH_TXEN_PIN         21
#define ETH_RXD0_PIN         25
#define ETH_RXD1_PIN         26
#define ETH_CRS_DV_PIN       27
