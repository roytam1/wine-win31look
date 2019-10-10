/*
 * msvcrt.dll C++ objects
 *
 * Copyright 2000 Jon Griffiths
 * Copyright 2003 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/exception.h"
#include "excpt.h"
#include "wine/debug.h"
#include "msvcrt/malloc.h"
#include "msvcrt/stdlib.h"

#include "msvcrt.h"
#include "cppexcept.h"
#include "mtdll.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

/*
 * exception object: base for exception, bad_cast, bad_typeid, __non_rtti_object
 */
typedef struct
{
  void* pfn_vector_dtor;
  void* pfn_what;
} exception_vtable;

typedef struct __exception
{
  const exception_vtable *vtable;
  char *name;    /* Name of this exception, always a new copy for each object */
  int   do_free; /* Whether to free 'name' in our dtor */
} exception;

typedef exception bad_cast;
typedef exception bad_typeid;
typedef exception __non_rtti_object;

typedef struct _rtti_base_descriptor
{
  type_info *type_descriptor;
  int num_base_classes;
  int base_class_offset;
  unsigned int flags;
  int unknown1;
  int unknown2;
} rtti_base_descriptor;

typedef struct _rtti_base_array
{
  const rtti_base_descriptor *bases[3]; /* First element is the class itself */
} rtti_base_array;

typedef struct _rtti_object_hierachy
{
  int unknown1;
  int unknown2;
  int array_len; /* Size of the array pointed to by 'base_classes' */
  const rtti_base_array *base_classes;
} rtti_object_hierachy;

typedef struct _rtti_object_locator
{
  int unknown1;
  int base_class_offset;
  unsigned int flags;
  type_info *type_descriptor;
  const rtti_object_hierachy *type_hierachy;
} rtti_object_locator;


#ifdef __i386__  /* thiscall functions are i386-specific */

#define DEFINE_THISCALL_WRAPPER(func) \
    extern void __thiscall_ ## func(); \
    __ASM_GLOBAL_FUNC(__thiscall_ ## func, \
                      "popl %eax\n\t" \
                      "pushl %ecx\n\t" \
                      "pushl %eax\n\t" \
                      "jmp " __ASM_NAME(#func) )

const exception_vtable MSVCRT_exception_vtable;
const exception_vtable MSVCRT_bad_typeid_vtable;
const exception_vtable MSVCRT_bad_cast_vtable;
const exception_vtable MSVCRT___non_rtti_object_vtable;
static const exception_vtable MSVCRT_type_info_vtable;

/* Internal common ctor for exception */
static void WINAPI EXCEPTION_ctor(exception *_this, const char** name)
{
  _this->vtable = &MSVCRT_exception_vtable;
  if (*name)
  {
    size_t name_len = strlen(*name) + 1;
    _this->name = MSVCRT_malloc(name_len);
    memcpy(_this->name, *name, name_len);
    _this->do_free = TRUE;
  }
  else
  {
    _this->name = NULL;
    _this->do_free = FALSE;
  }
}

/******************************************************************
 *		??0exception@@QAE@ABQBD@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_ctor);
exception * __stdcall MSVCRT_exception_ctor(exception * _this, const char ** name)
{
  TRACE("(%p,%s)\n", _this, *name);
  EXCEPTION_ctor(_this, name);
  return _this;
}

/******************************************************************
 *		??0exception@@QAE@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_copy_ctor);
exception * __stdcall MSVCRT_exception_copy_ctor(exception * _this, const exception * rhs)
{
  TRACE("(%p,%p)\n", _this, rhs);

  if (!rhs->do_free)
  {
    _this->vtable = &MSVCRT_exception_vtable;
    _this->name = rhs->name;
    _this->do_free = FALSE;
  }
  else
    EXCEPTION_ctor(_this, (const char**)&rhs->name);
  TRACE("name = %s\n", _this->name);
  return _this;
}

/******************************************************************
 *		??0exception@@QAE@XZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_default_ctor);
exception * __stdcall MSVCRT_exception_default_ctor(exception * _this)
{
  static const char* empty = NULL;

  TRACE("(%p)\n", _this);
  EXCEPTION_ctor(_this, &empty);
  return _this;
}

/******************************************************************
 *		??1exception@@UAE@XZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_dtor);
void __stdcall MSVCRT_exception_dtor(exception * _this)
{
  TRACE("(%p)\n", _this);
  _this->vtable = &MSVCRT_exception_vtable;
  if (_this->do_free) MSVCRT_free(_this->name);
}

/******************************************************************
 *		??4exception@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_opequals);
exception * __stdcall MSVCRT_exception_opequals(exception * _this, const exception * rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  if (_this != rhs)
  {
      MSVCRT_exception_dtor(_this);
      MSVCRT_exception_copy_ctor(_this, rhs);
  }
  TRACE("name = %s\n", _this->name);
  return _this;
}

/******************************************************************
 *		??_Eexception@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_vector_dtor);
void * __stdcall MSVCRT_exception_vector_dtor(exception * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    if (flags & 2)
    {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)_this - 1;

        for (i = *ptr - 1; i >= 0; i--) MSVCRT_exception_dtor(_this + i);
        MSVCRT_operator_delete(ptr);
    }
    else
    {
        MSVCRT_exception_dtor(_this);
        if (flags & 1) MSVCRT_operator_delete(_this);
    }
    return _this;
}

/******************************************************************
 *		??_Gexception@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_exception_scalar_dtor);
void * __stdcall MSVCRT_exception_scalar_dtor(exception * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    MSVCRT_exception_dtor(_this);
    if (flags & 1) MSVCRT_operator_delete(_this);
    return _this;
}

/******************************************************************
 *		?what@exception@@UBEPBDXZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_what_exception);
const char * __stdcall MSVCRT_what_exception(exception * _this)
{
  TRACE("(%p) returning %s\n", _this, _this->name);
  return _this->name ? _this->name : "Unknown exception";
}

/******************************************************************
 *		??0bad_typeid@@QAE@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_typeid_copy_ctor);
bad_typeid * __stdcall MSVCRT_bad_typeid_copy_ctor(bad_typeid * _this, const bad_typeid * rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  MSVCRT_exception_copy_ctor(_this, rhs);
  _this->vtable = &MSVCRT_bad_typeid_vtable;
  return _this;
}

/******************************************************************
 *		??0bad_typeid@@QAE@PBD@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_typeid_ctor);
bad_typeid * __stdcall MSVCRT_bad_typeid_ctor(bad_typeid * _this, const char * name)
{
  TRACE("(%p %s)\n", _this, name);
  EXCEPTION_ctor(_this, &name);
  _this->vtable = &MSVCRT_bad_typeid_vtable;
  return _this;
}

/******************************************************************
 *		??1bad_typeid@@UAE@XZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_typeid_dtor);
void __stdcall MSVCRT_bad_typeid_dtor(bad_typeid * _this)
{
  TRACE("(%p)\n", _this);
  MSVCRT_exception_dtor(_this);
}

/******************************************************************
 *		??4bad_typeid@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_typeid_opequals);
bad_typeid * __stdcall MSVCRT_bad_typeid_opequals(bad_typeid * _this, const bad_typeid * rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  MSVCRT_exception_opequals(_this, rhs);
  return _this;
}

/******************************************************************
 *              ??_Ebad_typeid@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_typeid_vector_dtor);
void * __stdcall MSVCRT_bad_typeid_vector_dtor(bad_typeid * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    if (flags & 2)
    {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)_this - 1;

        for (i = *ptr - 1; i >= 0; i--) MSVCRT_bad_typeid_dtor(_this + i);
        MSVCRT_operator_delete(ptr);
    }
    else
    {
        MSVCRT_bad_typeid_dtor(_this);
        if (flags & 1) MSVCRT_operator_delete(_this);
    }
    return _this;
}

/******************************************************************
 *		??_Gbad_typeid@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_typeid_scalar_dtor);
void * __stdcall MSVCRT_bad_typeid_scalar_dtor(bad_typeid * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    MSVCRT_bad_typeid_dtor(_this);
    if (flags & 1) MSVCRT_operator_delete(_this);
    return _this;
}

/******************************************************************
 *		??0__non_rtti_object@@QAE@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT___non_rtti_object_copy_ctor);
__non_rtti_object * __stdcall MSVCRT___non_rtti_object_copy_ctor(__non_rtti_object * _this,
                                                                 const __non_rtti_object * rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  MSVCRT_bad_typeid_copy_ctor(_this, rhs);
  _this->vtable = &MSVCRT___non_rtti_object_vtable;
  return _this;
}

/******************************************************************
 *		??0__non_rtti_object@@QAE@PBD@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT___non_rtti_object_ctor);
__non_rtti_object * __stdcall MSVCRT___non_rtti_object_ctor(__non_rtti_object * _this,
                                                            const char * name)
{
  TRACE("(%p %s)\n", _this, name);
  EXCEPTION_ctor(_this, &name);
  _this->vtable = &MSVCRT___non_rtti_object_vtable;
  return _this;
}

/******************************************************************
 *		??1__non_rtti_object@@UAE@XZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT___non_rtti_object_dtor);
void __stdcall MSVCRT___non_rtti_object_dtor(__non_rtti_object * _this)
{
  TRACE("(%p)\n", _this);
  MSVCRT_bad_typeid_dtor(_this);
}

/******************************************************************
 *		??4__non_rtti_object@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT___non_rtti_object_opequals);
__non_rtti_object * __stdcall MSVCRT___non_rtti_object_opequals(__non_rtti_object * _this,
                                                                const __non_rtti_object *rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  MSVCRT_bad_typeid_opequals(_this, rhs);
  return _this;
}

/******************************************************************
 *		??_E__non_rtti_object@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT___non_rtti_object_vector_dtor);
void * __stdcall MSVCRT___non_rtti_object_vector_dtor(__non_rtti_object * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    if (flags & 2)
    {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)_this - 1;

        for (i = *ptr - 1; i >= 0; i--) MSVCRT___non_rtti_object_dtor(_this + i);
        MSVCRT_operator_delete(ptr);
    }
    else
    {
        MSVCRT___non_rtti_object_dtor(_this);
        if (flags & 1) MSVCRT_operator_delete(_this);
    }
    return _this;
}

/******************************************************************
 *		??_G__non_rtti_object@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT___non_rtti_object_scalar_dtor);
void * __stdcall MSVCRT___non_rtti_object_scalar_dtor(__non_rtti_object * _this, unsigned int flags)
{
  TRACE("(%p %x)\n", _this, flags);
  MSVCRT___non_rtti_object_dtor(_this);
  if (flags & 1) MSVCRT_operator_delete(_this);
  return _this;
}

/******************************************************************
 *		??0bad_cast@@QAE@ABQBD@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_cast_ctor);
bad_cast * __stdcall MSVCRT_bad_cast_ctor(bad_cast * _this, const char ** name)
{
  TRACE("(%p %s)\n", _this, *name);
  EXCEPTION_ctor(_this, name);
  _this->vtable = &MSVCRT_bad_cast_vtable;
  return _this;
}

/******************************************************************
 *		??0bad_cast@@QAE@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_cast_copy_ctor);
bad_cast * __stdcall MSVCRT_bad_cast_copy_ctor(bad_cast * _this, const bad_cast * rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  MSVCRT_exception_copy_ctor(_this, rhs);
  _this->vtable = &MSVCRT_bad_cast_vtable;
  return _this;
}

/******************************************************************
 *		??1bad_cast@@UAE@XZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_cast_dtor);
void __stdcall MSVCRT_bad_cast_dtor(bad_cast * _this)
{
  TRACE("(%p)\n", _this);
  MSVCRT_exception_dtor(_this);
}

/******************************************************************
 *		??4bad_cast@@QAEAAV0@ABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_cast_opequals);
bad_cast * __stdcall MSVCRT_bad_cast_opequals(bad_cast * _this, const bad_cast * rhs)
{
  TRACE("(%p %p)\n", _this, rhs);
  MSVCRT_exception_opequals(_this, rhs);
  return _this;
}

/******************************************************************
 *              ??_Ebad_cast@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_cast_vector_dtor);
void * __stdcall MSVCRT_bad_cast_vector_dtor(bad_cast * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    if (flags & 2)
    {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)_this - 1;

        for (i = *ptr - 1; i >= 0; i--) MSVCRT_bad_cast_dtor(_this + i);
        MSVCRT_operator_delete(ptr);
    }
    else
    {
        MSVCRT_bad_cast_dtor(_this);
        if (flags & 1) MSVCRT_operator_delete(_this);
    }
    return _this;
}

/******************************************************************
 *		??_Gbad_cast@@UAEPAXI@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_bad_cast_scalar_dtor);
void * __stdcall MSVCRT_bad_cast_scalar_dtor(bad_cast * _this, unsigned int flags)
{
  TRACE("(%p %x)\n", _this, flags);
  MSVCRT_bad_cast_dtor(_this);
  if (flags & 1) MSVCRT_operator_delete(_this);
  return _this;
}

/******************************************************************
 *		??8type_info@@QBEHABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_opequals_equals);
int __stdcall MSVCRT_type_info_opequals_equals(type_info * _this, const type_info * rhs)
{
    int ret = !strcmp(_this->mangled + 1, rhs->mangled + 1);
    TRACE("(%p %p) returning %d\n", _this, rhs, ret);
    return ret;
}

/******************************************************************
 *		??9type_info@@QBEHABV0@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_opnot_equals);
int __stdcall MSVCRT_type_info_opnot_equals(type_info * _this, const type_info * rhs)
{
    int ret = !!strcmp(_this->mangled + 1, rhs->mangled + 1);
    TRACE("(%p %p) returning %d\n", _this, rhs, ret);
    return ret;
}

/******************************************************************
 *		?before@type_info@@QBEHABV1@@Z (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_before);
int __stdcall MSVCRT_type_info_before(type_info * _this, const type_info * rhs)
{
    int ret = strcmp(_this->mangled + 1, rhs->mangled + 1) < 0;
    TRACE("(%p %p) returning %d\n", _this, rhs, ret);
    return ret;
}

/******************************************************************
 *		??1type_info@@UAE@XZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_dtor);
void __stdcall MSVCRT_type_info_dtor(type_info * _this)
{
  TRACE("(%p)\n", _this);
  if (_this->name)
    MSVCRT_free(_this->name);
}

/******************************************************************
 *		?name@type_info@@QBEPBDXZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_name);
const char * __stdcall MSVCRT_type_info_name(type_info * _this)
{
  if (!_this->name)
  {
    /* Create and set the demangled name */
    char* name = MSVCRT___unDName(0, _this->mangled, 0,
                                  (MSVCRT_malloc_func)MSVCRT_malloc,
                                  (MSVCRT_free_func)MSVCRT_free, 0x2800);

    if (name)
    {
      unsigned int len = strlen(name);

      /* It seems _unDName may leave blanks at the end of the demangled name */
      if (name[len] == ' ')
        name[len] = '\0';

      _mlock(_EXIT_LOCK2);

      if (_this->name)
      {
        /* Another thread set this member since we checked above - use it */
        MSVCRT_free(name);
      }
      else
        _this->name = name;

      _munlock(_EXIT_LOCK2);
    }
  }
  TRACE("(%p) returning %s\n", _this, _this->name);
  return _this->name;
}

/******************************************************************
 *		?raw_name@type_info@@QBEPBDXZ (MSVCRT.@)
 */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_raw_name);
const char * __stdcall MSVCRT_type_info_raw_name(type_info * _this)
{
  TRACE("(%p) returning %s\n", _this, _this->mangled);
  return _this->mangled;
}

/* Unexported */
DEFINE_THISCALL_WRAPPER(MSVCRT_type_info_vector_dtor);
void * __stdcall MSVCRT_type_info_vector_dtor(type_info * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    if (flags & 2)
    {
        /* we have an array, with the number of elements stored before the first object */
        int i, *ptr = (int *)_this - 1;

        for (i = *ptr - 1; i >= 0; i--) MSVCRT_type_info_dtor(_this + i);
        MSVCRT_operator_delete(ptr);
    }
    else
    {
        MSVCRT_type_info_dtor(_this);
        if (flags & 1) MSVCRT_operator_delete(_this);
    }
    return _this;
}

/* vtables */

const exception_vtable MSVCRT_exception_vtable =
{
    __thiscall_MSVCRT_exception_vector_dtor,
    __thiscall_MSVCRT_what_exception
};

const exception_vtable MSVCRT_bad_typeid_vtable =
{
    __thiscall_MSVCRT_bad_typeid_vector_dtor,
    __thiscall_MSVCRT_what_exception
};

const exception_vtable MSVCRT_bad_cast_vtable =
{
    __thiscall_MSVCRT_bad_cast_vector_dtor,
    __thiscall_MSVCRT_what_exception
};

const exception_vtable MSVCRT___non_rtti_object_vtable =
{
    __thiscall_MSVCRT___non_rtti_object_vector_dtor,
    __thiscall_MSVCRT_what_exception
};

static const exception_vtable MSVCRT_type_info_vtable =
{
    __thiscall_MSVCRT_type_info_vector_dtor,
    NULL
};

/* Static RTTI for exported objects */

static type_info exception_type_info =
{
  (void*)&MSVCRT_type_info_vtable,
  NULL,
  ".?AVexception@@"
};

static const rtti_base_descriptor exception_rtti_base_descriptor =
{
  &exception_type_info,
  0,
  0,
  0,
  0,
  0
};

static const rtti_base_array exception_rtti_base_array =
{
  {
    &exception_rtti_base_descriptor,
    NULL,
    NULL
  }
};

static const rtti_object_hierachy exception_type_hierachy =
{
  0,
  0,
  1,
  &exception_rtti_base_array
};

static const rtti_object_locator exception_rtti =
{
  0,
  0,
  0,
  &exception_type_info,
  &exception_type_hierachy
};

static const cxx_type_info exception_cxx_type_info =
{
  0,
  &exception_type_info,
  0,
  -1,
  0,
  sizeof(exception),
  (cxx_copy_ctor)__thiscall_MSVCRT_exception_copy_ctor
};

static type_info bad_typeid_type_info =
{
  (void*)&MSVCRT_type_info_vtable,
  NULL,
  ".?AVbad_typeid@@"
};

static const rtti_base_descriptor bad_typeid_rtti_base_descriptor =
{
  &bad_typeid_type_info,
  1,
  0,
  0xffffffff,
  0,
  0
};

static const rtti_base_array bad_typeid_rtti_base_array =
{
  {
    &bad_typeid_rtti_base_descriptor,
    &exception_rtti_base_descriptor,
    NULL
  }
};

static const rtti_object_hierachy bad_typeid_type_hierachy =
{
  0,
  0,
  2,
  &bad_typeid_rtti_base_array
};

static const rtti_object_locator bad_typeid_rtti =
{
  0,
  0,
  0,
  &bad_typeid_type_info,
  &bad_typeid_type_hierachy
};

static const cxx_type_info bad_typeid_cxx_type_info =
{
  0,
  &bad_typeid_type_info,
  0,
  -1,
  0,
  sizeof(exception),
  (cxx_copy_ctor)__thiscall_MSVCRT_bad_typeid_copy_ctor
};

static type_info bad_cast_type_info =
{
  (void*)&MSVCRT_type_info_vtable,
  NULL,
  ".?AVbad_cast@@"
};

static const rtti_base_descriptor bad_cast_rtti_base_descriptor =
{
  &bad_cast_type_info,
  1,
  0,
  0xffffffff,
  0,
  0
};

static const rtti_base_array bad_cast_rtti_base_array =
{
  {
    &bad_cast_rtti_base_descriptor,
    &exception_rtti_base_descriptor,
    NULL
  }
};

static const rtti_object_hierachy bad_cast_type_hierachy =
{
  0,
  0,
  2,
  &bad_cast_rtti_base_array
};

static const rtti_object_locator bad_cast_rtti =
{
  0,
  0,
  0,
  &bad_cast_type_info,
  &bad_cast_type_hierachy
};

static const cxx_type_info bad_cast_cxx_type_info =
{
  0,
  &bad_cast_type_info,
  0,
  -1,
  0,
  sizeof(exception),
  (cxx_copy_ctor)__thiscall_MSVCRT_bad_cast_copy_ctor
};

static type_info __non_rtti_object_type_info =
{
  (void*)&MSVCRT_type_info_vtable,
  NULL,
  ".?AV__non_rtti_object@@"
};

static const rtti_base_descriptor __non_rtti_object_rtti_base_descriptor =
{
  &__non_rtti_object_type_info,
  2,
  0,
  0xffffffff,
  0,
  0
};

static const rtti_base_array __non_rtti_object_rtti_base_array =
{
  {
    &__non_rtti_object_rtti_base_descriptor,
    &bad_typeid_rtti_base_descriptor,
    &exception_rtti_base_descriptor
  }
};

static const rtti_object_hierachy __non_rtti_object_type_hierachy =
{
  0,
  0,
  3,
  &__non_rtti_object_rtti_base_array
};

static const rtti_object_locator __non_rtti_object_rtti =
{
  0,
  0,
  0,
  &__non_rtti_object_type_info,
  &__non_rtti_object_type_hierachy
};

static const cxx_type_info __non_rtti_object_cxx_type_info =
{
  0,
  &__non_rtti_object_type_info,
  0,
  -1,
  0,
  sizeof(exception),
  (cxx_copy_ctor)__thiscall_MSVCRT___non_rtti_object_copy_ctor
};

static type_info type_info_type_info =
{
  (void*)&MSVCRT_type_info_vtable,
  NULL,
  ".?AVtype_info@@"
};

static const rtti_base_descriptor type_info_rtti_base_descriptor =
{
  &type_info_type_info,
  0,
  0,
  0xffffffff,
  0,
  0
};

static const rtti_base_array type_info_rtti_base_array =
{
  {
    &type_info_rtti_base_descriptor,
    NULL,
    NULL
  }
};

static const rtti_object_hierachy type_info_type_hierachy =
{
  0,
  0,
  1,
  &type_info_rtti_base_array
};

static const rtti_object_locator type_info_rtti =
{
  0,
  0,
  0,
  &type_info_type_info,
  &type_info_type_hierachy
};

/*
 * Exception RTTI for cpp objects
 */
static const cxx_type_info_table bad_cast_type_info_table =
{
  3,
  {
   &__non_rtti_object_cxx_type_info,
   &bad_typeid_cxx_type_info,
   &exception_cxx_type_info
  }
};

static const cxx_exception_type bad_cast_exception_type =
{
  0,
  (void*)__thiscall_MSVCRT_bad_cast_dtor,
  NULL,
  &bad_cast_type_info_table
};

static const cxx_type_info_table bad_typeid_type_info_table =
{
  2,
  {
   &bad_cast_cxx_type_info,
   &exception_cxx_type_info,
   NULL
  }
};

static const cxx_exception_type bad_typeid_exception_type =
{
  0,
  (void*)__thiscall_MSVCRT_bad_typeid_dtor,
  NULL,
  &bad_cast_type_info_table
};

static const cxx_exception_type __non_rtti_object_exception_type =
{
  0,
  (void*)__thiscall_MSVCRT___non_rtti_object_dtor,
  NULL,
  &bad_typeid_type_info_table
};

#endif  /* __i386__ */


/******************************************************************
 *		?set_terminate@@YAP6AXXZP6AXXZ@Z (MSVCRT.@)
 *
 * Install a handler to be called when terminate() is called.
 *
 * PARAMS
 *  func [I] Handler function to install
 *
 * RETURNS
 *  The previously installed handler function, if any.
 */
terminate_function MSVCRT_set_terminate(terminate_function func)
{
    MSVCRT_thread_data *data = msvcrt_get_thread_data();
    terminate_function previous = data->terminate_handler;
    TRACE("(%p) returning %p\n",func,previous);
    data->terminate_handler = func;
    return previous;
}

/******************************************************************
 *		?set_unexpected@@YAP6AXXZP6AXXZ@Z (MSVCRT.@)
 *
 * Install a handler to be called when unexpected() is called.
 *
 * PARAMS
 *  func [I] Handler function to install
 *
 * RETURNS
 *  The previously installed handler function, if any.
 */
unexpected_function MSVCRT_set_unexpected(unexpected_function func)
{
    MSVCRT_thread_data *data = msvcrt_get_thread_data();
    unexpected_function previous = data->unexpected_handler;
    TRACE("(%p) returning %p\n",func,previous);
    data->unexpected_handler = func;
    return previous;
}

/******************************************************************
 *              ?_set_se_translator@@YAP6AXIPAU_EXCEPTION_POINTERS@@@ZP6AXI0@Z@Z  (MSVCRT.@)
 */
_se_translator_function MSVCRT__set_se_translator(_se_translator_function func)
{
    MSVCRT_thread_data *data = msvcrt_get_thread_data();
    _se_translator_function previous = data->se_translator;
    TRACE("(%p) returning %p\n",func,previous);
    data->se_translator = func;
    return previous;
}

/******************************************************************
 *		?terminate@@YAXXZ (MSVCRT.@)
 *
 * Default handler for an unhandled exception.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  This function does not return. Either control resumes from any
 *  handler installed by calling set_terminate(), or (by default) abort()
 *  is called.
 */
void MSVCRT_terminate(void)
{
    MSVCRT_thread_data *data = msvcrt_get_thread_data();
    if (data->terminate_handler) data->terminate_handler();
    MSVCRT_abort();
}

/******************************************************************
 *		?unexpected@@YAXXZ (MSVCRT.@)
 */
void MSVCRT_unexpected(void)
{
    MSVCRT_thread_data *data = msvcrt_get_thread_data();
    if (data->unexpected_handler) data->unexpected_handler();
    MSVCRT_terminate();
}

/* Get type info from an object (internal) */
static const rtti_object_locator* RTTI_GetObjectLocator(type_info *cppobj)
{
  const rtti_object_locator *obj_locator = NULL;

#ifdef __i386__
  const exception_vtable* vtable = (const exception_vtable*)cppobj->vtable;

  /* Perhaps this is one of classes we export? */
  if (vtable == &MSVCRT_exception_vtable)
  {
    TRACE("returning exception_rtti\n");
    return &exception_rtti;
  }
  else if (vtable == &MSVCRT_bad_typeid_vtable)
  {
    TRACE("returning bad_typeid_rtti\n");
    return &bad_typeid_rtti;
  }
  else if (vtable == &MSVCRT_bad_cast_vtable)
  {
    TRACE("returning bad_cast_rtti\n");
    return &bad_cast_rtti;
  }
  else if (vtable == &MSVCRT___non_rtti_object_vtable)
  {
    TRACE("returning __non_rtti_object_rtti\n");
    return &__non_rtti_object_rtti;
  }
  else if (vtable == &MSVCRT_type_info_vtable)
  {
    TRACE("returning type_info_rtti\n");
    return &type_info_rtti;
  }
#endif

  if (!IsBadReadPtr(cppobj, sizeof(void *)) &&
      !IsBadReadPtr(cppobj->vtable - 1,sizeof(void *)) &&
      !IsBadReadPtr((void*)cppobj->vtable[-1], sizeof(rtti_object_locator)))
  {
    obj_locator = (rtti_object_locator *)cppobj->vtable[-1];
    TRACE("returning type_info from vtable (%p)\n", obj_locator);
  }

  return obj_locator;
}

/******************************************************************
 *		__RTtypeid (MSVCRT.@)
 *
 * Retrieve the Run Time Type Information (RTTI) for a C++ object.
 *
 * PARAMS
 *  cppobj [I] C++ object to get type information for.
 *
 * RETURNS
 *  Success: A type_info object describing cppobj.
 *  Failure: If the object to be cast has no RTTI, a __non_rtti_object
 *           exception is thrown. If cppobj is NULL, a bad_typeid exception
 *           is thrown. In either case, this function does not return.
 *
 * NOTES
 *  This function is usually called by compiler generated code as a result
 *  of using one of the C++ dynamic cast statements.
 */
type_info* MSVCRT___RTtypeid(type_info *cppobj)
{
  const rtti_object_locator *obj_locator = RTTI_GetObjectLocator(cppobj);

#ifdef __i386__
  if (!obj_locator)
  {
    static const char* szNullPtr = "Attempted a typeid of NULL pointer!";
    static const char* szBadPtr = "Bad read pointer - no RTTI data!";
    const cxx_exception_type *e_type;
    exception e;

    /* Throw a bad_typeid or __non_rtti_object exception */
    if (!cppobj)
    {
      EXCEPTION_ctor(&e, &szNullPtr);
      e.vtable = &MSVCRT_bad_typeid_vtable;
      e_type = &bad_typeid_exception_type;
    }
    else
    {
      EXCEPTION_ctor(&e, &szBadPtr);
      e.vtable = &MSVCRT___non_rtti_object_vtable;
      e_type = &__non_rtti_object_exception_type;
    }

    _CxxThrowException(&e, e_type);
    DebugBreak();
  }
  return obj_locator->type_descriptor;
#else
  return NULL;
#endif
}

/******************************************************************
 *		__RTDynamicCast (MSVCRT.@)
 *
 * Dynamically cast a C++ object to one of its base classes.
 *
 * PARAMS
 *  cppobj   [I] Any C++ object to cast
 *  unknown  [I] Reserved, set to 0
 *  src      [I] type_info object describing cppobj
 *  dst      [I] type_info object describing the base class to cast to
 *  do_throw [I] TRUE = throw an exception if the cast fails, FALSE = don't
 *
 * RETURNS
 *  Success: The address of cppobj, cast to the object described by dst.
 *  Failure: NULL, If the object to be cast has no RTTI, or dst is not a
 *           valid cast for cppobj. If do_throw is TRUE, a bad_cast exception
 *           is thrown and this function does not return.
 *
 * NOTES
 *  This function is usually called by compiler generated code as a result
 *  of using one of the C++ dynamic cast statements.
 */
void* MSVCRT___RTDynamicCast(type_info *cppobj, int unknown,
                             type_info *src, type_info *dst,
                             int do_throw)
{
  const rtti_object_locator *obj_locator;

  /* Note: cppobj _isn't_ a type_info, we use that struct for its vtable ptr */
  TRACE("(%p,%d,%p,%p,%d)\n", cppobj, unknown, src, dst, do_throw);
  if (!cppobj)
    return 0;
  obj_locator= RTTI_GetObjectLocator(cppobj);
  if (unknown)
    FIXME("Unknown parameter is non-zero: please report\n");

  /* To cast an object at runtime:
   * 1.Find out the true type of the object from the typeinfo at vtable[-1]
   * 2.Search for the destination type in the class hierarchy
   * 3.If destination type is found, return base object address + dest offset
   *   Otherwise, fail the cast
   */
  if (obj_locator)
  {
    int count = 0;
    const rtti_object_hierachy *obj_bases = obj_locator->type_hierachy;
    const rtti_base_descriptor **base_desc = obj_bases->base_classes->bases;
    int src_offset = obj_locator->base_class_offset, dst_offset = -1;

    while (count < obj_bases->array_len)
    {
      const type_info *typ = (*base_desc)->type_descriptor;

      if (!strcmp(typ->mangled, dst->mangled))
      {
        dst_offset = (*base_desc)->base_class_offset;
        break;
      }
      base_desc++;
      count++;
    }
    if (dst_offset >= 0)
      return (void*)((unsigned long)cppobj - src_offset + dst_offset);
  }

#ifdef __i386__
  /* VC++ sets do_throw to 1 when the result of a dynamic_cast is assigned
   * to a reference, since references cannot be NULL.
   */
  if (do_throw)
  {
    static const char* exception_text = "Bad dynamic_cast!";
    exception e;

    /* Throw a bad_cast exception */
    EXCEPTION_ctor(&e, &exception_text);
    e.vtable = &MSVCRT_bad_cast_vtable;
    _CxxThrowException(&e, &bad_cast_exception_type);
    DebugBreak();
  }
#endif
  return NULL;
}


/******************************************************************
 *		__RTCastToVoid (MSVCRT.@)
 *
 * Dynamically cast a C++ object to a void*.
 *
 * PARAMS
 *  cppobj [I] The C++ object to cast
 *
 * RETURNS
 *  Success: The base address of the object as a void*.
 *  Failure: NULL, if cppobj is NULL or has no RTTI.
 *
 * NOTES
 *  This function is usually called by compiler generated code as a result
 *  of using one of the C++ dynamic cast statements.
 */
void* MSVCRT___RTCastToVoid(type_info *cppobj)
{
  const rtti_object_locator *obj_locator = RTTI_GetObjectLocator(cppobj);

  /* Note: cppobj _isn't_ a type_info, we use that struct for its vtable ptr */
  TRACE("(%p)\n", cppobj);

  /* Casts to void* simply cast to the base object */
  if (obj_locator)
    return (void*)((unsigned long)cppobj - obj_locator->base_class_offset);
  return NULL;
}
