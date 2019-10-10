/*
 * Useful functions for winegcc/winewrap
 *
 * Copyright 2000 Francois Gouget
 * Copyright 2002 Dimitrie O. Paun
 * Copyright 2003 Richard Cohen
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


#ifndef DECLSPEC_NORETURN
# if defined(_MSC_VER) && (_MSC_VER >= 1200) && !defined(MIDL_PASS)
#  define DECLSPEC_NORETURN __declspec(noreturn)
# elif defined(__GNUC__)
#  define DECLSPEC_NORETURN __attribute__((noreturn))
# else
#  define DECLSPEC_NORETURN
# endif
#endif

void error(const char* s, ...) DECLSPEC_NORETURN;

void* xmalloc(size_t size);
void* xrealloc(void* p, size_t size);
char* strmake(const char* fmt, ...);
int strendswith(const char* str, const char* end);

typedef struct {
    size_t maximum;
    size_t size;
    const char** base;
} strarray;

strarray* strarray_alloc(void);
strarray* strarray_dup(const strarray* arr);
void strarray_free(strarray* arr);
void strarray_add(strarray* arr, const char* str);
void strarray_del(strarray* arr, int i);
void strarray_addall(strarray* arr, const strarray* from);
strarray* strarray_fromstring(const char* str, const char* delim);
char* strarray_tostring(const strarray* arr, const char* sep);

typedef enum { 
    file_na, file_other, file_obj, file_res, file_rc, 
    file_arh, file_dll, file_so, file_def, file_spec
} file_type;

char* get_basename(const char* file);
void create_file(const char* name, int mode, const char* fmt, ...);
file_type get_file_type(const char* filename);
file_type get_lib_type(strarray* path, const char* library, char** file);
void spawn(const strarray* prefix, const strarray* arr);

extern int verbose;
