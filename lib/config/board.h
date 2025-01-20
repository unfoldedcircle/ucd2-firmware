// SPDX-FileCopyrightText: Copyright (c) 2022 Unfolded Circle ApS and/or its affiliates <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Board configuration definition
// Indicator LED, IR receiver and IR LED pins are setup in the corresponding classes

#if !defined(HW_REVISION)
#error Missing HW_REVISION build flag!
#endif

#define STRINGIFY(s) STRINGIFY1(s)
#define STRINGIFY1(s) #s

// Device model of firmware in case eFuse is not set
static const char* hwModel = STRINGIFY(HW_MODEL);
// Hardware revision of firmware in case eFuse is not set
static const char* hwRevision = STRINGIFY(HW_REVISION);

#if defined(HW_REVISION_3)
#include "board_3.h"
#elif defined(HW_REVISION_4)
#include "board_4.h"
#elif defined(HW_REVISION_5_2)
#include "board_5_2.h"
#elif defined(HW_REVISION_5_3)
#include "board_5_3.h"
#elif defined(HW_REVISION_5_4)
#include "board_5_4.h"
#else
#error You need to specify a board revision in the build flags: \
HW_REVISION_5_4, HW_REVISION_5_3, HW_REVISION_5_2, HW_REVISION_4, HW_REVISION_3
#endif
