/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2001 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_INTR_H
#define _ASM_IA64_SN_INTR_H

#include <linux/config.h>

#if defined(CONFIG_IA64_SGI_SN1)
#include <asm/sn/sn1/intr.h>
#elif defined(CONFIG_IA64_SGI_SN2)
#include <asm/sn/sn2/intr.h>
#endif

#endif /* _ASM_IA64_SN_INTR_H */
