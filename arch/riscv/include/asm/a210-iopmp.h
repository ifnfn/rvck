/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 *    *** IMPORTANT ***
 * This file is not only included from C-code but also from devicetree source
 * files. As such this file MUST only contain comments and defines.
 *
 * Copyright (c) 2025 Xuliang Lin <linxuliang@zhcomputing.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _A210_IOPMP_H
#define _A210_IOPMP_H

long iopmp_enable(u32 *device_ids, u32 count);

long iopmp_disable(u32 *device_ids, u32 count);

#endif
