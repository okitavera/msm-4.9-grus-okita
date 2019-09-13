/*
 * Input Assisted SchedTune Boost
 *
 * Copyright (c) 2019 Nanda Oktavera <codeharuka.yusa@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IA_SCHEDTUNE_BOOST_H
#define _IA_SCHEDTUNE_BOOST_H

#ifdef CONFIG_IA_SCHEDTUNE_BOOST
void iasb_exec_boost(unsigned int duration_ms);
void iasb_kill_boost(void);
#endif
#endif
