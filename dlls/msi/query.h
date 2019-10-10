/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002 Mike McCormack for CodeWeavers
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

#ifndef __WINE_MSI_QUERY_H
#define __WINE_MSI_QUERY_H

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objidl.h"
#include "msi.h"
#include "msiquery.h"
#include "msipriv.h"


#define OP_EQ       1
#define OP_AND      2
#define OP_OR       3
#define OP_GT       4
#define OP_LT       5
#define OP_LE       6
#define OP_GE       7
#define OP_NE       8
#define OP_ISNULL   9
#define OP_NOTNULL  10

#define EXPR_COMPLEX  1
#define EXPR_COLUMN   2
#define EXPR_COL_NUMBER 3
#define EXPR_IVAL     4
#define EXPR_SVAL     5
#define EXPR_UVAL     6

typedef struct _string_list
{
    LPWSTR string;
    struct _string_list *next;
} string_list;

struct complex_expr
{
    UINT op;
    struct expr *left;
    struct expr *right;
};

struct expr
{
    int type;
    union
    {
        struct complex_expr expr;
        INT   ival;
        UINT  uval;
        LPWSTR sval;
        LPWSTR column;
        UINT col_number;
    } u;
};

typedef struct _create_col_info
{
    LPWSTR colname;
    UINT   type;
    struct _create_col_info *next;
} create_col_info;

typedef struct _value_list
{
    struct expr *val;
    struct _value_list *next;
} value_list;

UINT MSI_ParseSQL( MSIDATABASE *db, LPCWSTR command, MSIVIEW **phView);

UINT TABLE_CreateView( MSIDATABASE *db, LPCWSTR name, MSIVIEW **view );

UINT SELECT_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table );
UINT SELECT_AddColumn( MSIVIEW *select, LPWSTR name );

UINT DISTINCT_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table );

UINT ORDER_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table );
UINT ORDER_AddColumn( MSIVIEW *group, LPWSTR name );

UINT WHERE_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table );
UINT WHERE_AddCondition( MSIVIEW *view, struct expr *condition );

UINT CREATE_CreateView( MSIDATABASE *db, MSIVIEW **view, LPWSTR table,
                        create_col_info *col_info, BOOL temp );

UINT INSERT_CreateView( MSIDATABASE *db, MSIVIEW **view, LPWSTR table,
                        string_list *columns, value_list *values, BOOL temp );

void delete_expr( struct expr *e );
void delete_string_list( string_list *sl );
void delete_value_list( value_list *vl );

int sqliteGetToken(const WCHAR *z, int *tokenType);

#endif /* __WINE_MSI_QUERY_H */
