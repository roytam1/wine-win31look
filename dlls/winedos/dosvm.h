/*
 * DOS virtual machine
 *
 * Copyright 2000 Ove K�ven
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_DOSVM_H
#define __WINE_DOSVM_H

#include <sys/types.h> /* pid_t */

typedef struct {
  PAPCFUNC proc;
  ULONG_PTR arg;
} DOS_SPC;

extern pid_t dosvm_pid;

#endif /* __WINE_DOSVM_H */
