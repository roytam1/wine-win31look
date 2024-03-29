/*
 * IDL Compiler
 *
 * Copyright 2004 Ove Kaaven
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

#ifndef __WIDL_TYPELIB_H
#define __WIDL_TYPELIB_H

extern int in_typelib;
extern void start_typelib(char *name, attr_t *attrs);
extern void end_typelib(void);
extern void add_interface(type_t *iface);
extern void add_coclass(class_t *cls);
extern void add_module(type_t *module);

#endif
